print("[Avalonium] [HUD] Universal HUD Toggle Mod loaded.")
print("[Avalonium] [HUD] F7 — Toggle HUD & UI visibility")
print("[Avalonium] [HUD] F9 — Force refresh HUD cache (after level loads)")

local TOGGLE_KEYS  = {"F7",}
local REFRESH_KEY  = "F9"

local function OnToggleHUD()
    local hidden = ToggleHUD()
    if hidden then
        print("[Avalonium] [HUD] All UI & HUD hidden")
    else
        print("[Avalonium] [HUD] All UI & HUD restored")
    end
end

local function OnRefreshCache()
    RefreshHUDCache()
    print("[Avalonium] [HUD] Cache refreshed — next toggle will rescan all widgets")
end

RegisterHook("LoadMap", function()
    RefreshHUDCache()

    if IsHUDHidden() then
        HideAllHUD()
    end
end)

for _, key in ipairs(TOGGLE_KEYS) do
    RegisterKeyBind(key, OnToggleHUD)
end
RegisterKeyBind(REFRESH_KEY, OnRefreshCache)
