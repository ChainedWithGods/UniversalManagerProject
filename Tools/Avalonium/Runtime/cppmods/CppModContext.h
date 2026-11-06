#pragma once

namespace Avalonium::CppMods
{

    inline void*& CurrentModTokenRef()
    {
        static thread_local void* token = nullptr;
        return token;
    }

    inline void* CurrentModToken()
    {
        return CurrentModTokenRef();
    }

    class ModContextScope
    {
      public:
        explicit ModContextScope(void* token) : m_Previous(CurrentModTokenRef())
        {
            CurrentModTokenRef() = token;
        }

        ~ModContextScope()
        {
            CurrentModTokenRef() = m_Previous;
        }

        ModContextScope(const ModContextScope&) = delete;
        ModContextScope& operator=(const ModContextScope&) = delete;

      private:
        void* m_Previous;
    };
}
