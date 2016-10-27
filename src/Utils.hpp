/**
 * FBIde project
 */
#pragma once

namespace fbide {
    
    // c++14 string literal "hello"s
    using namespace std::literals::string_literals;

    
    /**
     * Hash map of string to T
     */
    template<class T>
    using StringMap = std::unordered_map<wxString, T>;
    
    
    /**
     * Concatinate path component together separated by platform specific path
	 * component separator
     */
    inline wxString operator / (const wxString & lhs, const wxString & rhs)
    {
        return wxString(lhs).append(wxFILE_SEP_PATH).append(rhs);
    }


	/**
	 * Append path component separated by platform specific path component separator
	 */
	inline wxString & operator /= (wxString & lhs, const wxString & rhs)
	{
		return lhs.append(wxFILE_SEP_PATH).append(rhs);
	}
    
    
    /**
     * wxString shorthand. "Hello"_wx
     */
    inline wxString operator "" _wx (const char * s, size_t len)
    {
        return {s, len};
    }

    
    /**
     * is_one_of checks if type T is one of the given types
     */
    template<typename T>
    constexpr bool is_one_of()
    {
        return false;
    }
    
    template<typename T, typename U, typename... R>
    constexpr bool is_one_of()
    {
        return std::is_same<std::decay_t<T>, U>::value || is_one_of<T, R...>();
    }

    
    /**
     * Disallow copying this class
     */
    class NonCopyable
    {
    protected:
        NonCopyable() {};
        ~NonCopyable() {};
        
    private:
        NonCopyable(const NonCopyable &) = delete;
        NonCopyable & operator = (const NonCopyable &) = delete;
    };
    
}
