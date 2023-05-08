// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*++
Module Name:
- cppwinrt_utils.h

Abstract:
- This module is used for winrt event declarations/definitions

Author(s):
- Carlos Zamora (CaZamor) 23-Apr-2019

Revision History:
- N/A
--*/

#pragma once

// This is a helper macro to make declaring events easier.
// This will declare the event handler and the methods for adding and removing a
// handler callback from the event
#define DECLARE_EVENT(name, eventHandler, args)          \
public:                                                  \
    winrt::event_token name(const args& handler);        \
    void name(const winrt::event_token& token) noexcept; \
                                                         \
protected:                                               \
    winrt::event<args> eventHandler;

// This is a helper macro for defining the body of events.
// Winrt events need a method for adding a callback to the event and removing
//      the callback. This macro will define them both for you, because they
//      don't really vary from event to event.
#define DEFINE_EVENT(className, name, eventHandler, args)                                         \
    winrt::event_token className::name(const args& handler) { return eventHandler.add(handler); } \
    void className::name(const winrt::event_token& token) noexcept { eventHandler.remove(token); }

// This is a helper macro to make declaring events easier.
// This will declare the event handler and the methods for adding and removing a
// handler callback from the event.
// Use this if you have a Windows.Foundation.TypedEventHandler
#define DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(name, eventHandler, sender, args)                  \
public:                                                                                           \
    winrt::event_token name(const Windows::Foundation::TypedEventHandler<sender, args>& handler); \
    void name(const winrt::event_token& token) noexcept;                                          \
                                                                                                  \
private:                                                                                          \
    winrt::event<Windows::Foundation::TypedEventHandler<sender, args>> eventHandler;

// This is a helper macro for defining the body of events.
// Winrt events need a method for adding a callback to the event and removing
//      the callback. This macro will define them both for you, because they
//      don't really vary from event to event.
// Use this if you have a Windows.Foundation.TypedEventHandler
#define DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(className, name, eventHandler, sender, args)                                                        \
    winrt::event_token className::name(const Windows::Foundation::TypedEventHandler<sender, args>& handler) { return eventHandler.add(handler); } \
    void className::name(const winrt::event_token& token) noexcept { eventHandler.remove(token); }

// This is a helper macro for both declaring the signature of an event, and
// defining the body. Winrt events need a method for adding a callback to the
// event and removing the callback. This macro will both declare the method
// signatures and define them both for you, because they don't really vary from
// event to event.
// Use this in a classes header if you have a Windows.Foundation.TypedEventHandler
#define TYPED_EVENT(name, sender, args)                                                                                                            \
public:                                                                                                                                            \
    winrt::event_token name(const winrt::Windows::Foundation::TypedEventHandler<sender, args>& handler) { return _##name##Handlers.add(handler); } \
    void name(const winrt::event_token& token) { _##name##Handlers.remove(token); }                                                                \
                                                                                                                                                   \
private:                                                                                                                                           \
    winrt::event<winrt::Windows::Foundation::TypedEventHandler<sender, args>> _##name##Handlers;

// This is a helper macro for both declaring the signature of a callback (nee event) and
// defining the body. Winrt callbacks need a method for adding a delegate to the
// callback and removing the delegate. This macro will both declare the method
// signatures and define them both for you, because they don't really vary from
// event to event.
// Use this in a class's header if you have a "delegate" type in your IDL.
#define WINRT_CALLBACK(name, args)                                                          \
public:                                                                                     \
    winrt::event_token name(const args& handler) { return _##name##Handlers.add(handler); } \
    void name(const winrt::event_token& token) { _##name##Handlers.remove(token); }         \
                                                                                            \
protected:                                                                                  \
    winrt::event<args> _##name##Handlers;

// This is a helper macro for both declaring the signature and body of an event
// which is exposed by one class, but actually handled entirely by one of the
// class's members. This type of event could be considered "forwarded" or
// "proxied" to the handling type. Case in point: many of the events on App are
// just forwarded straight to TerminalPage. This macro will both declare the
// method signatures and define them both for you.
#define FORWARDED_TYPED_EVENT(name, sender, args, handler, handlerName)                                                        \
public:                                                                                                                        \
    winrt::event_token name(const Windows::Foundation::TypedEventHandler<sender, args>& h) { return handler->handlerName(h); } \
    void name(const winrt::event_token& token) noexcept { handler->handlerName(token); }

// Same thing, but handler is a projected type, not an implementation
#define PROJECTED_FORWARDED_TYPED_EVENT(name, sender, args, handler, handlerName)                                             \
public:                                                                                                                       \
    winrt::event_token name(const Windows::Foundation::TypedEventHandler<sender, args>& h) { return handler.handlerName(h); } \
    void name(const winrt::event_token& token) noexcept { handler.handlerName(token); }

// Use this macro to quick implement both the getter and setter for a property.
// This should only be used for simple types where there's no logic in the
// getter/setter beyond just accessing/updating the value.
#define WINRT_PROPERTY(type, name, ...)                        \
public:                                                        \
    type name() const noexcept { return _##name; }             \
    void name(const type& value) noexcept { _##name = value; } \
                                                               \
private:                                                       \
    type _##name{ __VA_ARGS__ };

// Use this macro to quickly implement both the getter and setter for an
// observable property. This is similar to the WINRT_PROPERTY macro above,
// except this will also raise a PropertyChanged event with the name of the
// property that has changed inside of the setter. This also implements a
// private _setName() method, that the class can internally use to change the
// value when it _knows_ it doesn't need to raise the PropertyChanged event
// (like when the class is being initialized).
#define WINRT_OBSERVABLE_PROPERTY(type, name, event, ...)                                 \
public:                                                                                   \
    type name() const noexcept { return _##name; };                                       \
    void name(const type& value)                                                          \
    {                                                                                     \
        if (_##name != value)                                                             \
        {                                                                                 \
            _##name = value;                                                              \
            event(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L## #name }); \
        }                                                                                 \
    };                                                                                    \
                                                                                          \
private:                                                                                  \
    type _##name{ __VA_ARGS__ };                                                          \
    void _set##name(const type& value)                                                    \
    {                                                                                     \
        _##name = value;                                                                  \
    };

// Use this macro for quickly defining the factory_implementation part of a
// class. CppWinrt requires these for the compiler, but more often than not,
// they require no customization. See
// https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/implements#marker-types
// and https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/static-lifetime
// for examples of when you might _not_ want to use this.
#define BASIC_FACTORY(typeName)                                       \
    struct typeName : typeName##T<typeName, implementation::typeName> \
    {                                                                 \
    };

// This is a helper method for deserializing a SAFEARRAY of
// COM objects and converting it to a vector that
// owns the extracted COM objects
template<typename T>
std::vector<wil::com_ptr<T>> SafeArrayToOwningVector(SAFEARRAY* safeArray)
{
    T** pVals;
    THROW_IF_FAILED(SafeArrayAccessData(safeArray, (void**)&pVals));

    THROW_HR_IF(E_UNEXPECTED, SafeArrayGetDim(safeArray) != 1);

    long lBound, uBound;
    THROW_IF_FAILED(SafeArrayGetLBound(safeArray, 1, &lBound));
    THROW_IF_FAILED(SafeArrayGetUBound(safeArray, 1, &uBound));

    long count = uBound - lBound + 1;

    // If any of the above fail, we cannot destruct/release
    // any of the elements in the SAFEARRAY because we
    // cannot identify how many elements there are.

    std::vector<wil::com_ptr<T>> result{ gsl::narrow<std::size_t>(count) };
    for (int i = 0; i < count; i++)
    {
        result[i].attach(pVals[i]);
    }

    return result;
}

#define DECLARE_CONVERTER(nameSpace, className)                                                                   \
    namespace nameSpace::implementation                                                                           \
    {                                                                                                             \
        struct className : className##T<className>                                                                \
        {                                                                                                         \
            className() = default;                                                                                \
                                                                                                                  \
            Windows::Foundation::IInspectable Convert(const Windows::Foundation::IInspectable& value,             \
                                                      const Windows::UI::Xaml::Interop::TypeName& targetType,     \
                                                      const Windows::Foundation::IInspectable& parameter,         \
                                                      const hstring& language);                                   \
                                                                                                                  \
            Windows::Foundation::IInspectable ConvertBack(const Windows::Foundation::IInspectable& value,         \
                                                          const Windows::UI::Xaml::Interop::TypeName& targetType, \
                                                          const Windows::Foundation::IInspectable& parameter,     \
                                                          const hstring& language);                               \
        };                                                                                                        \
    }                                                                                                             \
                                                                                                                  \
    namespace nameSpace::factory_implementation                                                                   \
    {                                                                                                             \
        BASIC_FACTORY(className);                                                                                 \
    }\
