/*
IDASDK extension library (c) Elias Bachaalany.

Kernwin utilities
*/
#pragma once

#include <functional>
#include <vector>
#include <memory>

#include <kernwin.hpp>

//----------------------------------------------------------------------------------
using update_state_ah_t = std::function<action_state_t(action_update_ctx_t*, bool)>;
using activate_ah_t     = std::function<int(action_activation_ctx_t*)>;

// Utility class to allow using function objects instead of inheriting from action_handler_t each time
struct fo_action_handler_ah_t: public action_handler_t
{
    qstring name;
    const char* popup_path;
    update_state_ah_t f_update;
    activate_ah_t f_activate;
    
    fo_action_handler_ah_t(const char *name, update_state_ah_t f_update, activate_ah_t f_activate,
        const char* popup_path = nullptr) :
        name(name), f_update(f_update), f_activate(f_activate), popup_path(popup_path) { }

    action_state_t idaapi update(action_update_ctx_t* ctx) override
    {
        return f_update(ctx, false);
    }

    virtual int idaapi activate(action_activation_ctx_t* ctx) override
    {
        return f_activate(ctx);
    }

    action_state_t idaapi get_state(TWidget *widget)
    {
        return f_update((action_update_ctx_t*)widget, true);
    }
};

using fo_action_handler_vec_t = std::vector<fo_action_handler_ah_t*>;

//----------------------------------------------------------------------------------
// Create a lambda version of an action handler
#define FO_ACTION_UPDATE(captures, update) \
    captures(action_update_ctx_t *ctx, bool is_widget)->action_state_t \
    { \
        TWidget *widget = is_widget ? (TWidget *)ctx : ctx->widget; \
        update \
    }

#define FO_ACTION_ACTIVATE(captures) \
    captures(action_activation_ctx_t *ctx)->int

//--------------------------------------------------------------------------
class action_manager_t
{
    objcontainer_t<fo_action_handler_ah_t> action_handlers;
    objcontainer_t<qstring> popup_paths;

    fo_action_handler_vec_t want_hxe_popup;
    fo_action_handler_vec_t want_ida_popup;
    const void* plg_owner;
    const char *current_popup_path = nullptr;

public:
    void set_popup_path(const char* path=nullptr)
    {
        if (path == nullptr)
            current_popup_path = nullptr;
        else
            current_popup_path = popup_paths.create(path)->c_str();
    }

    ssize_t on_ui_finish_populating_widget_popup(va_list va)
    {
        TWidget* widget          = va_arg(va, TWidget*);
        TPopupMenu* popup_handle = va_arg(va, TPopupMenu*);
        maybe_attach_to_popup(false, widget, popup_handle);
        return 0;
    }

    ssize_t on_hxe_populating_popup(va_list va)
    {
        TWidget* widget   = va_arg(va, TWidget*);
        TPopupMenu* popup = va_arg(va, TPopupMenu*);
        maybe_attach_to_popup(true, widget, popup);
        return 0;

    }

    void maybe_attach_to_popup(
        bool via_hxe,
        TWidget* widget,
        TPopupMenu* popup_handle,
        const char* popuppath = nullptr,
        int flags = 0)
    {
        auto& lst = via_hxe ? want_hxe_popup : want_ida_popup;
        for (auto& act : lst)
        {
            if (is_action_enabled(act->get_state(widget)))
            {
                attach_action_to_popup(
                    widget, 
                    popup_handle, 
                    act->name.c_str(), 
                    popuppath == nullptr ? act->popup_path : popuppath, 
                    flags);
            }
        }
    }

    action_manager_t(const void* owner = nullptr) : plg_owner(owner) { }
    action_handler_t *add_action(
        int amflags,
#define AMAHF_NONE          0x00
#define AMAHF_HXE_POPUP     0x01
#define AMAHF_IDA_POPUP     0x04
        const char* name,
        const char* label,
        const char* shortcut,
        update_state_ah_t f_update,
        activate_ah_t  f_activate,
        const char* tooltip = nullptr,
        int icon = -1)
    {
        bool ok = register_action(ACTION_DESC_LITERAL_PLUGMOD(
            name,
            label,
            action_handlers.create(name, f_update, f_activate, current_popup_path),
            plg_owner,
            shortcut,
            tooltip,
            icon));

        fo_action_handler_ah_t* act = nullptr;
        
        if (ok)
        {
            act = action_handlers[-1];
            if (amflags & AMAHF_HXE_POPUP)
                want_hxe_popup.push_back(act);
            if (amflags & AMAHF_IDA_POPUP)
                want_ida_popup.push_back(act);
        }

        return act;
    }
};