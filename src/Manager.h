/**
 * FBIde project
 */
#pragma once

namespace fbide {
    
    class ManagerBase;
    class Manager;
    class UiManager;
    
    /**
     * Get manager instance.
     *
     * This is shorthand for Manager::GetInstance()
     */
    Manager & GetMgr();
    
    /**
     * Get UI manager
     *
     * This is shorthand for Manager::GetInstance().GetUiManager()
     */
    UiManager & GetUiMgr();
    

    /**
     * Main manager class. This is aproxy class that holds
     * the instances and other bookkeeping of the SDK
     * and should be used to access the SDK API
     *
     * This class is a singleton
     */
    class Manager : private NonCopyable
    {
    public:
        
        // Get manager instance
        static Manager & GetInstance();
        
        // clean up
        static void Release();
        
        // Load everything
        void Load();
        
        // Get UI manager
        UiManager & GetUiManager() const;
        
    private:
        
        Manager();
        ~Manager();
    };
    
    
    // macro to declare a manager class in the header
    #define DECLARE_MANAGER(_class) \
        private : \
            friend class Manager; \
            static _class & GetInstance (); \
            static void Release ();
        
        // Macro to implement Manager class logic in the source
    #define IMPLEMENT_MANAGER(_class) \
        namespace { _class * _p_manager_##_class = nullptr; } \
        _class & _class::GetInstance() { \
            if ( _p_manager_##_class == nullptr ) {\
                _p_manager_##_class = new _class(); \
            } \
            return *_p_manager_##_class; \
        } \
        void _class::Release () { \
            if (_p_manager_##_class == nullptr ) return; \
            delete _p_manager_##_class; \
            _p_manager_##_class = nullptr; \
        }
}
