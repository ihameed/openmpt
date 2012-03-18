#pragma once

#include <windows.h>
#include "../pervasives/pervasives.h"

namespace modplug {
namespace gui {

struct widget_t;

widget_t *create_widget();

typedef POINT point_t;
typedef POINTS points_t;
typedef RECT rect_t;
typedef CREATESTRUCT createstruct_t;
typedef HDC hdc_t;

namespace tarded {
#define COMMA ,
#define EMPTY

#ifndef _MSC_VER


template<bool has_impl, typename return_type = void>
struct enable_if {
    typedef return_type type;
};
template<typename return_type>
struct enable_if<false, return_type> { };

#define IF_FUNC_DEFINED(the_guy) if (tarded::has_##the_guy<widget_state_t>::value)

#define GETTYPE typeof //decltype
#define CHECK_HAS_THE_GUY(the_guy, decl_list, invoke_list) \
template<typename a_guy> struct has_##the_guy { \
    typedef char ya[1]; \
    typedef char neg[2]; \
    template<typename might_have_the_guy> static ya  &test(GETTYPE(&might_have_the_guy::the_guy)); \
    template<typename might_have_the_guy> static neg &test(...); \
    static bool const value = sizeof(test<a_guy>(0)) == sizeof(ya); \
}; \
template<typename a_guy> \
typename enable_if<has_##the_guy<a_guy>::value, int>::type do_##the_guy(a_guy *guy decl_list) { \
    return guy->the_guy(invoke_list); \
} \
template<typename a_guy> \
typename enable_if<!has_##the_guy<a_guy>::value, int>::type do_##the_guy(a_guy *guy decl_list) { \
    return 0; \
}

#undef GETTYPE


#else


#define CHECK_HAS_THE_GUY(the_guy, decl_list, invoke_list) \
template<typename a_guy> \
int do_##the_guy(a_guy *guy decl_list) { \
    __if_exists(a_guy::the_guy) { \
        guy->the_guy(invoke_list); \
    } \
    return 0; \
}

#define IF_FUNC_DEFINED(the_guy) \
__if_exists(widget_state_t::the_guy)



#endif //_MSC_VER

CHECK_HAS_THE_GUY(resized,          COMMA int width COMMA int height, width COMMA height);
CHECK_HAS_THE_GUY(lmb_down,         COMMA points_t pos, pos);
CHECK_HAS_THE_GUY(lmb_double_click, COMMA points_t pos, pos);
CHECK_HAS_THE_GUY(lmb_up,           EMPTY, EMPTY);
CHECK_HAS_THE_GUY(rmb_up,           EMPTY, EMPTY);
CHECK_HAS_THE_GUY(mouse_moved,      COMMA points_t pos, pos);
CHECK_HAS_THE_GUY(paint,            EMPTY, EMPTY);

#undef COMMA
#undef EMPTY
#undef CHECK_HAS_THE_GUY
}



template <typename wndproc_state_t>
wndproc_state_t *_get_wndproc_state(HWND hwnd, wndproc_state_t *&state) {
    state = reinterpret_cast<wndproc_state_t *>(GetWindowLongPtr(hwnd, 0));
    return state;
}

template <typename wndproc_state_t>
wndproc_state_t *_set_wndproc_state(HWND hwnd, wndproc_state_t *state) {
    SetLastError(0);
    auto ret = reinterpret_cast<wndproc_state_t *>(SetWindowLongPtr(hwnd, 0, reinterpret_cast<LONG_PTR>(state)));
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return ret;
}


template <typename widget_state_t>
inline LRESULT default_wndproc(HWND &hwnd, UINT &msg, WPARAM &wparam, LPARAM &lparam) {
    widget_state_t *state;
    CREATESTRUCT *params;
    state = _get_wndproc_state(hwnd, state);
    switch (msg) {
    case WM_NCCREATE:
        params = reinterpret_cast<CREATESTRUCT *>(lparam);
        if (!params) return FALSE;

        state = reinterpret_cast<widget_state_t *>(params->lpCreateParams);
        _set_wndproc_state(hwnd, state);
        if (GetLastError()) {
            modplug::pervasives::debug_log("failed to assign structure with err %d", GetLastError());
            return FALSE;
        }

        state->init(hwnd);

        //IF_FUNC_DEFINED(resized) { return tarded::do_resized(state, params->cx - params->x, params->cy - params->y); }

        SetWindowText(hwnd, params->lpszName);

        return TRUE;

    case WM_PAINT:
        IF_FUNC_DEFINED(paint) { return tarded::do_paint(state); }
        break;

    case WM_SIZE:
        IF_FUNC_DEFINED(resized) { return tarded::do_resized(state, LOWORD(lparam), HIWORD(lparam)); }
        break;

    case WM_LBUTTONUP:
        IF_FUNC_DEFINED(lmb_up) { return tarded::do_lmb_up(state); }
        break;

    case WM_LBUTTONDBLCLK:
        IF_FUNC_DEFINED(lmb_double_click) { return tarded::do_lmb_double_click(state, MAKEPOINTS(lparam)); }
        break;

    case WM_LBUTTONDOWN:
        IF_FUNC_DEFINED(lmb_down) { return tarded::do_lmb_down(state, MAKEPOINTS(lparam)); }
        break;

    case WM_RBUTTONUP:
        IF_FUNC_DEFINED(rmb_up) { return tarded::do_rmb_up(state); }
        break;

    case WM_MOUSEMOVE:
        IF_FUNC_DEFINED(mouse_moved) { return tarded::do_mouse_moved(state, MAKEPOINTS(lparam)); }
        break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

#undef IF_FUNC_DEFINED
#define DEFINE_WNDPROC(state_type_name) \
LRESULT CALLBACK wndproc_##state_type_name(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) { \
    return default_wndproc<state_type_name>(hwnd, msg, wparam, lparam); \
}




}
}