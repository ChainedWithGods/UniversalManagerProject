#pragma once

#include "Api.hpp"
#include "Value.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace RC::Unreal
{
    using Avalonium::FName;
    using Avalonium::FString;
    using Avalonium::Value;

    class UObject;
    class UStruct;
    class UClass;
    class UFunction;

    struct PropertyInfo
    {
        std::string Name;
        std::string TypeName;
        int32_t Offset = 0;
        int32_t ElementSize = 0;
        int32_t ArrayDim = 1;
        uint64_t Flags = 0;
    };

    struct FunctionInfo
    {
        std::string Name;
        UFunction* Function = nullptr;
        uint32_t Flags = 0;
        int32_t FrameSize = 0;
    };

    class UObject
    {
      public:
        UObject() = delete;
        ~UObject() = delete;

        static UObject* FindObject(std::string_view path)
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->find_object) return nullptr;
            return static_cast<UObject*>(api->find_object(std::string(path).c_str()));
        }

        static UClass* FindClass(std::string_view name);

        static UObject* Construct(UClass* objectClass, UObject* outer = nullptr)
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->construct_object || !objectClass) return nullptr;
            return static_cast<UObject*>(
                api->construct_object(AddressOfRaw(objectClass), AddressOfRaw(outer)));
        }

        static UObject* GetWorld()
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_world) return nullptr;
            return static_cast<UObject*>(api->get_world());
        }

        static UObject* FindFirstOfClass(UClass* objectClass)
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->find_first_object_of_class || !objectClass) return nullptr;
            return static_cast<UObject*>(api->find_first_object_of_class(AddressOfRaw(objectClass)));
        }

        static int32_t GetObjectCount()
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            return (api && api->get_object_count) ? api->get_object_count() : 0;
        }

        static UObject* GetObjectByIndex(int32_t index)
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_object_by_index) return nullptr;
            return static_cast<UObject*>(api->get_object_by_index(index));
        }

        static int32_t GetObjectsOfClass(UClass* objectClass, std::vector<UObject*>& out, int32_t maxObjects = 4096)
        {
            out.clear();
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->find_objects_of_class || !objectClass || maxObjects <= 0) return 0;

            std::vector<void*> pointers(static_cast<size_t>(maxObjects), nullptr);
            const int32_t count = api->find_objects_of_class(AddressOfRaw(objectClass), pointers.data(), maxObjects);
            out.reserve(static_cast<size_t>(count > 0 ? count : 0));
            for (int32_t i = 0; i < count; ++i)
                out.push_back(static_cast<UObject*>(pointers[static_cast<size_t>(i)]));
            return count;
        }

        static void* AddressOf(const UObject* object)
        {
            return const_cast<void*>(static_cast<const void*>(object));
        }

        static void* AddressOfRaw(const void* object)
        {
            return const_cast<void*>(object);
        }

        void* Address() const { return AddressOf(this); }

        bool IsValid() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            return api && api->is_valid_object && api->is_valid_object(Address());
        }

        FString GetName() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            const char* text = (api && api->get_object_name) ? api->get_object_name(Address()) : nullptr;
            return FString(text ? text : "");
        }

        FString GetFullName() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            const char* text = (api && api->get_object_full_name) ? api->get_object_full_name(Address()) : nullptr;
            return FString(text ? text : "");
        }

        FString GetPathName() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            const char* text = (api && api->get_object_path_name) ? api->get_object_path_name(Address()) : nullptr;
            return FString(text ? text : "");
        }

        UClass* GetClass() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_object_class) return nullptr;
            return static_cast<UClass*>(api->get_object_class(Address()));
        }

        UObject* GetOuter() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_object_outer) return nullptr;
            return static_cast<UObject*>(api->get_object_outer(Address()));
        }

        bool IsA(std::string_view className) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->is_a) return false;
            return api->is_a(Address(), std::string(className).c_str());
        }

        UFunction* FindFunction(std::string_view functionName) const;

        bool GetPropertyValue(std::string_view propertyName, Value& out) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_property) return false;
            AvaloniumValue raw{};
            if (!api->get_property(Address(), std::string(propertyName).c_str(), &raw)) return false;
            out = Value::FromRaw(raw);
            return true;
        }

        bool SetPropertyValue(std::string_view propertyName, const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_property) return false;
            return api->set_property(Address(), std::string(propertyName).c_str(), &value.Raw());
        }

        template <class T>
        T GetProperty(std::string_view propertyName) const
        {
            Value value;
            if (!GetPropertyValue(propertyName, value)) return T{};

            if constexpr (std::is_pointer_v<T>)
            {
                using Pointee = std::remove_pointer_t<T>;
                if constexpr (std::is_same_v<Pointee, void> || std::is_base_of_v<UObject, Pointee>)
                    return static_cast<T>(value.AsObject());
                else
                    return T{};
            }
            else
            {
                return value.As<T>();
            }
        }

        template <class T>
        bool SetProperty(std::string_view propertyName, const T& value) const
        {
            return SetPropertyValue(propertyName, Avalonium::ToValue(value));
        }

        bool GetStructProperty(std::string_view propertyName, void* out, int32_t outSize) const
        {
            Value value;
            return GetPropertyValue(propertyName, value) && value.CopyStruct(out, outSize);
        }

        bool SetStructProperty(std::string_view propertyName, const void* data, int32_t size) const
        {
            return SetPropertyValue(propertyName, Value::Struct(data, size));
        }

        bool CallValues(std::string_view functionName, const std::vector<Value>& args, Value* result = nullptr) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->call_function) return false;

            std::vector<AvaloniumValue> rawArgs;
            rawArgs.reserve(args.size());
            for (const Value& arg : args) rawArgs.push_back(arg.Raw());

            AvaloniumValue rawResult{};
            const bool ok = api->call_function(Address(), std::string(functionName).c_str(),
                                               rawArgs.empty() ? nullptr : rawArgs.data(),
                                               static_cast<int32_t>(rawArgs.size()), &rawResult);
            if (ok && result) *result = Value::FromRaw(rawResult);
            return ok;
        }

        template <class R = void, class... Args>
        R Call(std::string_view functionName, Args&&... args) const
        {
            std::vector<Value> values;
            values.reserve(sizeof...(Args));
            (values.emplace_back(Avalonium::ToValue(std::forward<Args>(args))), ...);

            Value result;
            if (!CallValues(functionName, values, &result))
            {
                if constexpr (!std::is_void_v<R>) return R{};
                else return;
            }

            if constexpr (std::is_void_v<R>)
            {
                return;
            }
            else if constexpr (std::is_pointer_v<R>)
            {
                using Pointee = std::remove_pointer_t<R>;
                if constexpr (std::is_same_v<Pointee, void> || std::is_base_of_v<UObject, Pointee>)
                    return static_cast<R>(result.AsObject());
                else
                    return R{};
            }
            else
            {
                return result.As<R>();
            }
        }

        bool ProcessEvent(UFunction* function, void* params) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->process_event || !function) return false;
            return api->process_event(Address(), AddressOfRaw(function), params);
        }

        int32_t GetArrayLength(std::string_view propertyName) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->array_length) return -1;
            return api->array_length(Address(), std::string(propertyName).c_str());
        }

        int32_t GetArrayElementSize(std::string_view propertyName) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->array_element_size) return 0;
            return api->array_element_size(Address(), std::string(propertyName).c_str());
        }

        bool GetArrayElement(std::string_view propertyName, int32_t index, Value& out) const
        {
            out = Value();
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->array_get) return false;
            AvaloniumValue raw{};
            if (!api->array_get(Address(), std::string(propertyName).c_str(), index, &raw))
                return false;
            out = Value::FromRaw(raw);
            return true;
        }

        bool SetArrayElement(std::string_view propertyName, int32_t index,
                             const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->array_set) return false;
            return api->array_set(Address(), std::string(propertyName).c_str(), index,
                                  &value.Raw());
        }

        bool AddArrayElement(std::string_view propertyName, const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->array_add) return false;
            return api->array_add(Address(), std::string(propertyName).c_str(), &value.Raw());
        }

        bool RemoveArrayElement(std::string_view propertyName, int32_t index) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->array_remove) return false;
            return api->array_remove(Address(), std::string(propertyName).c_str(), index);
        }

        int32_t GetMapLength(std::string_view propertyName) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_length) return -1;
            return api->map_length(Address(), std::string(propertyName).c_str());
        }

        bool GetMapEntry(std::string_view propertyName, int32_t index, Value& outKey,
                         Value& outValue) const
        {
            outKey = Value();
            outValue = Value();
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_get_at) return false;
            AvaloniumValue rawKey{};
            AvaloniumValue rawValue{};
            if (!api->map_get_at(Address(), std::string(propertyName).c_str(), index, &rawKey,
                                 &rawValue))
                return false;
            outKey = Value::FromRaw(rawKey);
            outValue = Value::FromRaw(rawValue);
            return true;
        }

        bool SetMapValue(std::string_view propertyName, int32_t index,
                         const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_set_value_at) return false;
            return api->map_set_value_at(Address(), std::string(propertyName).c_str(), index,
                                         &value.Raw());
        }

        int32_t GetSetLength(std::string_view propertyName) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_length) return -1;
            return api->set_length(Address(), std::string(propertyName).c_str());
        }

        bool GetSetElement(std::string_view propertyName, int32_t index, Value& out) const
        {
            out = Value();
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_get_at) return false;
            AvaloniumValue raw{};
            if (!api->set_get_at(Address(), std::string(propertyName).c_str(), index, &raw))
                return false;
            out = Value::FromRaw(raw);
            return true;
        }

        bool AddMapEntry(std::string_view propertyName, const Value& key,
                         const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_add) return false;
            return api->map_add(Address(), std::string(propertyName).c_str(), &key.Raw(),
                                &value.Raw());
        }

        bool RemoveMapEntry(std::string_view propertyName, const Value& key) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_remove) return false;
            return api->map_remove(Address(), std::string(propertyName).c_str(), &key.Raw());
        }

        bool ClearMap(std::string_view propertyName) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_clear) return false;
            return api->map_clear(Address(), std::string(propertyName).c_str());
        }

        bool ContainsMapKey(std::string_view propertyName, const Value& key) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_contains) return false;
            return api->map_contains(Address(), std::string(propertyName).c_str(),
                                     &key.Raw());
        }

        bool FindMapValue(std::string_view propertyName, const Value& key, Value& out) const
        {
            out = Value();
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->map_find) return false;
            AvaloniumValue raw{};
            if (!api->map_find(Address(), std::string(propertyName).c_str(), &key.Raw(), &raw))
                return false;
            out = Value::FromRaw(raw);
            return true;
        }

        bool AddSetElement(std::string_view propertyName, const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_add) return false;
            return api->set_add(Address(), std::string(propertyName).c_str(), &value.Raw());
        }

        bool RemoveSetElement(std::string_view propertyName, const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_remove) return false;
            return api->set_remove(Address(), std::string(propertyName).c_str(),
                                   &value.Raw());
        }

        bool ClearSet(std::string_view propertyName) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_clear) return false;
            return api->set_clear(Address(), std::string(propertyName).c_str());
        }

        bool ContainsSetElement(std::string_view propertyName, const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_contains) return false;
            return api->set_contains(Address(), std::string(propertyName).c_str(),
                                     &value.Raw());
        }

        bool GetPropertyPath(std::string_view path, Value& out) const
        {
            out = Value();
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_property_path) return false;
            AvaloniumValue raw{};
            if (!api->get_property_path(Address(), std::string(path).c_str(), &raw))
                return false;
            out = Value::FromRaw(raw);
            return true;
        }

        bool SetPropertyPath(std::string_view path, const Value& value) const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->set_property_path) return false;
            return api->set_property_path(Address(), std::string(path).c_str(),
                                          &value.Raw());
        }

      protected:
        static UFunction* FindFunctionOn(const void* objectClass, std::string_view functionName)
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->find_function || !objectClass) return nullptr;
            return static_cast<UFunction*>(api->find_function(AddressOfRaw(objectClass), std::string(functionName).c_str()));
        }
    };

    class UStruct : public UObject
    {
      public:
        UStruct() = delete;
        ~UStruct() = delete;

        UStruct* GetSuperStruct() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_super_class) return nullptr;
            return static_cast<UStruct*>(api->get_super_class(Address()));
        }

        std::vector<PropertyInfo> GetProperties() const
        {
            std::vector<PropertyInfo> properties;
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_class_properties) return properties;

            constexpr int32_t kBatch = 1024;
            std::vector<AvaloniumPropertyInfo> buffer(kBatch);
            const int32_t count = api->get_class_properties(Address(), buffer.data(), kBatch);
            properties.reserve(static_cast<size_t>(count > 0 ? count : 0));
            for (int32_t i = 0; i < count; ++i)
            {
                const AvaloniumPropertyInfo& info = buffer[static_cast<size_t>(i)];
                PropertyInfo property;
                property.Name = info.name;
                property.TypeName = info.type_name;
                property.Offset = info.offset;
                property.ElementSize = info.element_size;
                property.ArrayDim = info.array_dim;
                property.Flags = info.flags;
                properties.push_back(std::move(property));
            }
            return properties;
        }
    };

    class UClass : public UStruct
    {
      public:
        UClass() = delete;
        ~UClass() = delete;

        UFunction* FindFunction(std::string_view functionName) const
        {
            return FindFunctionOn(this, functionName);
        }

        UObject* GetDefaultObject() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_class_default_object) return nullptr;
            return static_cast<UObject*>(api->get_class_default_object(Address()));
        }

        std::vector<FunctionInfo> GetFunctions() const
        {
            std::vector<FunctionInfo> functions;
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            if (!api || !api->get_class_functions) return functions;

            constexpr int32_t kBatch = 2048;
            std::vector<AvaloniumFunctionInfo> buffer(kBatch);
            const int32_t count = api->get_class_functions(Address(), buffer.data(), kBatch);
            functions.reserve(static_cast<size_t>(count > 0 ? count : 0));
            for (int32_t i = 0; i < count; ++i)
            {
                const AvaloniumFunctionInfo& info = buffer[static_cast<size_t>(i)];
                FunctionInfo function;
                function.Name = info.name;
                function.Function = static_cast<UFunction*>(info.function);
                function.Flags = info.function_flags;
                function.FrameSize = info.frame_size;
                functions.push_back(std::move(function));
            }
            return functions;
        }
    };

    class UFunction : public UStruct
    {
      public:
        UFunction() = delete;
        ~UFunction() = delete;

        uint32_t GetFunctionFlags() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            return (api && api->get_function_flags) ? api->get_function_flags(Address()) : 0;
        }

        int32_t GetParametersSize() const
        {
            const AvaloniumModApi* api = Avalonium::Detail::Api();
            return (api && api->get_function_frame_size) ? api->get_function_frame_size(Address()) : 0;
        }

        bool IsNative() const
        {

            return (GetFunctionFlags() & 0x00000400u) != 0;
        }
    };

    inline UClass* UObject::FindClass(std::string_view name)
    {
        const AvaloniumModApi* api = Avalonium::Detail::Api();
        if (!api || !api->find_class) return nullptr;
        return static_cast<UClass*>(api->find_class(std::string(name).c_str()));
    }

    inline UFunction* UObject::FindFunction(std::string_view functionName) const
    {
        UClass* objectClass = GetClass();
        return objectClass ? objectClass->FindFunction(functionName) : nullptr;
    }

    inline UObject* FindObject(std::string_view path)
    {
        return UObject::FindObject(path);
    }

    inline UClass* FindClass(std::string_view name)
    {
        return UObject::FindClass(name);
    }

    inline UObject* FindFirstOf(std::string_view className)
    {
        UClass* objectClass = UObject::FindClass(className);
        return objectClass ? UObject::FindFirstOfClass(objectClass) : nullptr;
    }
}
