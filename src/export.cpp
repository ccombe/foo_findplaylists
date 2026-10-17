#include "stdafx.h"
#include "export.h"
#include "logic.h"
#include "scanner.h"

static std::string utf8_from_wide(const wchar_t *w) {
    if (!w || !*w)
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1)
        return {};
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

[[nodiscard]] bool pick_folder(std::string &out) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(co);
    bool ok = false;
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        if (uninit)
            CoUninitialize();
        return false;
    }

    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts)))
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    if (SUCCEEDED(dlg->Show(core_api::get_main_window()))) {
        IShellItem *item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                out = utf8_from_wide(path);
                ok = !out.empty();
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    if (uninit)
        CoUninitialize();
    return ok;
}

[[nodiscard]] int pick_format() {
    const TASKDIALOG_BUTTON buttons[] = {
        {100, L"M3U8\nUTF-8 playlist, works with Rekordbox"},
        {101, L"M3U\nLegacy ANSI playlist"},
    };
    TASKDIALOGCONFIG cfg = {};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = core_api::get_main_window();
    cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    cfg.pszWindowTitle = L"Export All Playlists";
    cfg.pszMainInstruction = L"Choose playlist file format";
    cfg.cButtons = 2;
    cfg.pButtons = buttons;
    int pressed = 0;
    if (FAILED(TaskDialogIndirect(&cfg, &pressed, nullptr, nullptr)))
        return 1;
    if (pressed == 100)
        return 1;
    if (pressed == 101)
        return 2;
    return 0;
}

[[nodiscard]] std::string prompt_save_playlist_path(const char *playlistName) {
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(co);
    std::string chosen;
    IFileSaveDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        if (uninit)
            CoUninitialize();
        return chosen;
    }

    const COMDLG_FILTERSPEC filters[] = {
        {L"M3U8 Playlist", L"*.m3u8"},
        {L"M3U Playlist", L"*.m3u"},
    };
    dlg->SetFileTypes(2, filters);
    dlg->SetFileTypeIndex(1);
    pfc::string_formatter sug;
    sug << safe_filename(playlistName).c_str() << ".m3u8";
    wchar_t wsug[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, sug.c_str(), -1, wsug, MAX_PATH);
    dlg->SetFileName(wsug);
    if (SUCCEEDED(dlg->Show(core_api::get_main_window()))) {
        IShellItem *item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                chosen = utf8_from_wide(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    if (uninit)
        CoUninitialize();
    return chosen;
}

void run_export_all_impl(pfc::string_base &msg) {
    std::string folder;
    if (!pick_folder(folder)) {
        msg << "Export cancelled.\n";
        return;
    }
    const int fmt = pick_format();
    if (fmt == 0) {
        msg << "Export cancelled.\n";
        return;
    }
    const char *ext = (fmt == 2) ? ".m3u" : ".m3u8";
    static_api_ptr_t<playlist_manager> pm;
    abort_callback_dummy abort;
    const t_size npl = pm->get_playlist_count();
    metadb_handle_list items;
    pfc::string_formatter plname, path;
    t_size ok = 0, skipped = 0;
    pfc::string_formatter body;
    for (t_size p = 0; p < npl; ++p) {
        items.remove_all();
        pm->playlist_get_all_items(p, items);
        plname.reset();
        if (!pm->playlist_get_name(p, plname))
            continue;
        if (is_managed_playlist(plname.c_str())) {
            ++skipped;
            continue;
        }
        path.reset();
        path << folder.c_str() << "\\" << safe_filename(plname.c_str()).c_str() << ext;
        try {
            playlist_loader::g_save_playlist(path.c_str(), items, abort);
            ++ok;
        } catch (const std::exception &e) {
            body << "FAILED: " << plname.c_str() << " (" << e.what() << ")\n";
        } catch (...) {
            body << "FAILED: " << plname.c_str() << "\n";
        }
    }
    pfc::string_formatter head;
    head << "Exported " << ok << " of " << npl << " playlists (" << ext << ") to " << folder.c_str() << "\n";
    if (skipped > 0)
        head << "(skipped " << skipped << " generated working lists)\n";
    head << body.c_str();
    msg << head.c_str();
}

void run_export_active_impl(pfc::string_base &msg) {
    static_api_ptr_t<playlist_manager> pm;
    pfc::string_formatter plname;
    if (!pm->activeplaylist_get_name(plname)) {
        msg << "No active playlist.\n";
        return;
    }

    const std::string chosen = prompt_save_playlist_path(plname.c_str());
    if (chosen.empty()) {
        msg << "Export cancelled.\n";
        return;
    }

    metadb_handle_list items;
    pm->activeplaylist_get_all_items(items);
    abort_callback_dummy abort;
    try {
        playlist_loader::g_save_playlist(chosen.c_str(), items, abort);
        msg << "Exported '" << plname.c_str() << "' (" << items.get_count() << " tracks) to " << chosen.c_str()
            << "\n";
    } catch (const std::exception &e) {
        msg << "FAILED: '" << plname.c_str() << "' (" << e.what() << ")\n";
    } catch (...) {
        msg << "FAILED: '" << plname.c_str() << "'\n";
    }
}
