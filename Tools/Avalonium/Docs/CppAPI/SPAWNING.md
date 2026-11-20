# Spawning and Mod Content

This tutorial uses the CppAPI additions in ABI 12 through 16. Each one answers a
question that a spawn workflow runs into:

| Question | API |
| :--- | :--- |
| What did the game's spawn function actually create? | `UObject::CallOutValues` |
| Is the result already scheduled for destruction? | `UObject::IsPendingKill` |
| Can the game be stopped from destroying it during init? | `Hook::RegisterVeto`, `Hook::RegisterAddressVeto` |
| Does this item or its assets come from a local mod pak? | `UObject::IsModAsset` |

The table only grows. A mod built against a newer SDK needs a runtime whose table
is at least as new; `AvaloniumGetModApi` returns null instead of a short table.

These pages assume a built and loaded mod. [Build Your First C++ Mod](../Installation/FirstCppMod.md)
covers the project, build and deploy steps, and [Installation](../Installation/Installation.md)
covers the runtime itself.

---

## 1. Read the caller's out parameters

`CallValues` runs a function and reads its return value. Spawn functions often
return nothing useful and write the actor to an out parameter instead.
`CallOutValues` resolves the names before the call, reads the caller's out storage
after it, and fails without running the function when a name does not exist.

```cpp
using namespace RC::Unreal;

std::vector<Value> args{
    Value::Object(state),
    Value::Struct(&transform, sizeof(transform))};

std::vector<Value> outValues;
if (!state->CallOutValues("Spawn Item Ex", args, {"act"}, outValues) ||
    outValues.empty())
{
    return nullptr;
}

auto* actor = static_cast<UObject*>(outValues.front().AsObject());
```

The optional last argument captures the function's return value:

```cpp
Value result;
state->CallOutValues("Spawn Item Ex", args, {"act"}, outValues, &result);
```

Blueprint functions do not all write back the same way. A function that copies its
out value into the caller's parameter block is visible here. One that writes only
its engine-local frame reads null from `CallOutValues`; a post hook on the same
UFunction can read that local with `GetArg`. A successful call means the function
ran, not that the out value is non-null.

---

## 2. Check liveness before using the result

`IsValid` says the memory is readable. `IsPendingKill` is the engine's own
destruction state. A destroyed actor still passes `IsValid` and keeps its property
values, but every reflected call on it returns without running.

```cpp
auto* actor = static_cast<RC::Unreal::UObject*>(outValues.front().AsObject());
if (!actor || !actor->IsValid() || actor->IsPendingKill())
{
    return nullptr;
}
```

The runtime reads the object array flags for both layouts: pending kill on UE4 and
garbage on UE5. A spawn that returns a readable actor and then reports pending kill
was destroyed by the game during its own initialization, not by a failed spawn.

A cached pointer outlives its object. After a level restart the old actor stays
readable, leaves the object array (its flags then read as zero, so `IsPendingKill`
reports false) and collapses to a `None` path. Reject cached objects by path as
well, and re-resolve them per level.

---

## 3. Veto a destroy during the spawn window

Some games spawn an actor and destroy it again inside its own initialization. The
destroy can bypass the UFunction entry (`K2_DestroyActor`) and call the native
`AActor::Destroy` directly, so a UFunction veto alone may miss it. The CppAPI
offers both routes.

A UFunction veto:

```cpp
static bool WhileSpawning(void* user, void* object, void* function,
                          void* frame, void* result)
{
    auto* mod = static_cast<MyMod*>(user);
    return !mod->IsSpawning();
}

uint32_t vetoId =
    RC::Hook::RegisterVeto("Actor", "K2_DestroyActor", &WhileSpawning, this);
```

An address veto for the path that never reaches the UFunction:

```cpp
const uintptr_t destroyRva = FindDestroyRva(); // pattern or signature scan
void* destroy = static_cast<uint8_t*>(RC::Native::Module()) + destroyRva;
uint32_t addressVetoId =
    RC::Hook::RegisterAddressVeto(destroy, &WhileSpawning, this);
```

Rules that keep a veto safe:

- A veto is a hard skip. The caller continues as if the function ran, so only veto
  a call whose cancellation leaves a valid state.
- `RegisterVeto` covers native UFunctions. An interpreted (Blueprint) function has
  no native entry to skip; use an address veto on the function the bytecode calls,
  or another route.
- The address must be the function entry, and the offset belongs to one engine
  build. Resolve it with `RC::Native::FindPattern` or a signature for the build you
  target; an address veto does not follow a game update.
- Hold the veto only for the spawn window. Clear the flag in every exit path of the
  spawn call, including failures.
- Register once and unregister in `on_unload` with `RC::Hook::Unregister(vetoId)`.

---

## 4. Mark content that comes from a local pak

A local pak under `<game>/Content/Paks/~mods` can mount into the game's own asset
paths, so a package path cannot tell a re-skin from base content. `IsModAsset`
resolves the object's outermost package against the pak set:

```cpp
bool IsModContent(RC::Unreal::UObject* object)
{
    return object && object->IsValid() && object->IsModAsset();
}
```

The runtime scans `~mods` once, reads each pak's mount point and file list, maps
them to package paths and caches the set. Two cases match:

- A package the mod adds, such as a new item class or data table.
- A game package whose asset a mod pak overrides, such as a weapon material or
  icon texture.

Check the item itself and the assets it displays. A base weapon wearing a skin
pak is still a base actor; its icon and materials are what identify it:

```cpp
entry.IsMod = component->IsModAsset();
if (!entry.IsMod && entry.Icon)
    entry.IsMod = entry.Icon->IsModAsset();
```

A mod that mounts under a mod-specific path such as `/Game/Mod/...` is also caught
by a path test. The pak check covers the paks that mount into the game's own
paths.

The index is built on the first call. An encrypted pak index is skipped, and a
mount added after that call is not in the set.

---

## 5. Putting it together

The spawn window combines the out parameter, liveness and veto APIs:

```cpp
bool MyMod::SpawnItem(RC::Unreal::UObject* state,
                      const std::vector<RC::Unreal::Value>& args)
{
    if (!EnsureDestroyGuard()) return false;

    std::vector<RC::Unreal::Value> outValues;
    m_Spawning = true;
    const bool ran = state->CallOutValues("Spawn Item Ex", args, {"act"}, outValues);
    m_Spawning = false;

    if (!ran || outValues.empty()) return false;

    auto* actor =
        static_cast<RC::Unreal::UObject*>(outValues.front().AsObject());
    return actor && actor->IsValid() && !actor->IsPendingKill();
}
```

`EnsureDestroyGuard` registers the UFunction veto, the address veto, or both, once
per process. The full guard class is in [EXAMPLES.md](EXAMPLES.md#6-destruction-guard).
If the game can reach the destroy from another thread, make `m_Spawning` a
`std::atomic<bool>`; the veto callback runs on the thread that makes the call.

---

## 6. Checklist

- Verify the spawn through the out parameter; a successful call is not enough.
- Reject a null, unreadable or pending-kill actor before storing it.
- Check the icon and mesh packages for mod provenance, not only the item path.
- Keep an address veto scoped to the spawn window and to one engine build.
- Unregister hooks and vetoes in `on_unload`.
