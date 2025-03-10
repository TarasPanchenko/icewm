/*
 * IceWM
 *
 * Copyright (C) 1997-2003 Marko Macek
 *
 * Window list
 */
#include "config.h"
#include "wmwinlist.h"
#include "ymenuitem.h"
#include "prefs.h"
#include "wmframe.h"
#include "wmmgr.h"
#include "wmapp.h"
#include "workspaces.h"
#include "wpixmaps.h"
#include <assert.h>
#include "intl.h"

WindowListProxy windowList;

WindowListItem::WindowListItem(ClientData *frame, int workspace):
    fFrame(frame),
    fWorkspace(workspace)
{
}

WindowListItem::~WindowListItem() {
    if (fFrame) {
        fFrame->setWinListItem(nullptr);
        fFrame = nullptr;
    }
}

int WindowListItem::getOffset() {
    int ofs = -20;
    for (ClientData *w = getFrame(); w; w = w->owner()) {
        ofs = max(20, ofs + 20);
    }
    return ofs;
}

mstring WindowListItem::getText() {
    if (fFrame)
        return getFrame()->getTitle();
    else if (fWorkspace < 0)
        return _("All Workspaces");
    else if (fWorkspace < workspaceCount)
        return workspaceNames[fWorkspace];
    else
        return null;
}

ref<YIcon> WindowListItem::getIcon() {
    if (fFrame)
        return getFrame()->getIcon();
    return null;
}


WindowListBox::WindowListBox(YScrollView *view, YWindow *aParent):
    YListBox(view, aParent)
{
}

WindowListBox::~WindowListBox() {
}

void WindowListBox::activateItem(YListItem *item) {
    WindowListItem *i = (WindowListItem *)item;
    ClientData *f = i->getFrame();
    if (f) {
        f->activateWindow(true, false);
        windowList->getFrame()->wmHide();
    } else {
        int w = i->getWorkspace();
        if (w >= 0) {
            manager->activateWorkspace(w);
            windowList->getFrame()->wmHide();
        }
    }
}

YArrange WindowListBox::getSelectedWindows() {
    YFrameWindow** frames = nullptr;
    int count = 0;
    if (hasSelection()) {
        for (IterType iter(getIterator()); ++iter; ) {
            count += isSelected(iter.where());
        }
    }
    if (count) {
        frames = new YFrameWindow*[count];
        int k = 0;
        for (IterType iter(getIterator()); ++iter; ) {
            if (isSelected(iter.where())) {
                WindowListItem *item = static_cast<WindowListItem *>(*iter);
                ClientData *frame = item->getFrame();
                if (frame && k < count) {
                    frames[k++] = static_cast<YFrameWindow *>(frame);
                }
            }
        }
    }
    return YArrange(frames, count);
}

void WindowListBox::actionPerformed(YAction action, unsigned int modifiers) {
    bool save = focusCurrentWorkspace;
    if (save) focusCurrentWorkspace = false;

    YArrange arrange = getSelectedWindows();
    if (arrange == false) {
    }
    else if (action == actionTileVertical) {
        manager->tileWindows(arrange, true);
    }
    else if (action == actionTileHorizontal) {
        manager->tileWindows(arrange, false);
    }
    else if (action == actionCascade) {
        manager->cascadePlace(arrange);
    }
    else if (action == actionArrange) {
        manager->smartPlace(arrange);
    }
    else {
        for (YFrameWindow* frame : arrange) {
            if ((action != actionHide || !frame->isHidden()) &&
                (action != actionMinimize || !frame->isMinimized())) {
                frame->actionPerformed(action, modifiers);
            }
        }
    }

    if (save) focusCurrentWorkspace = save;
}

bool WindowListBox::handleKey(const XKeyEvent &key) {
    if (key.type == KeyPress) {
        KeySym k = keyCodeToKeySym(key.keycode);
        int m = KEY_MODMASK(key.state);

        switch (k) {
        case XK_Escape:
            windowList->getFrame()->wmHide();
            return true;
        case XK_F10:
        case XK_Menu:
            if (k != XK_F10 || m == ShiftMask) {
                if (hasSelection()) {
                    YMenu* windowListPopup = windowList->getWindowListPopup();
                    enableCommands(windowListPopup);
                    windowListPopup->popup(nullptr, nullptr, nullptr,
                                           key.x_root, key.y_root,
                                           YPopupWindow::pfCanFlipVertical |
                                           YPopupWindow::pfCanFlipHorizontal |
                                           YPopupWindow::pfPopupMenu);
                } else {
                    YMenu* windowListAllPopup = windowList->getWindowListAllPopup();
                    windowListAllPopup->popup(nullptr, nullptr, nullptr,
                                              key.x_root, key.y_root,
                                              YPopupWindow::pfCanFlipVertical |
                                              YPopupWindow::pfCanFlipHorizontal |
                                              YPopupWindow::pfPopupMenu);
                }
            }
            break;
        case XK_Delete:
            {
                if (m & ShiftMask)
                    actionPerformed(actionKill, key.state);
                else
                    actionPerformed(actionClose, key.state);
            }
            break;
        }
    }
    return YListBox::handleKey(key);
}

void WindowListBox::handleClick(const XButtonEvent &up, int count) {
    if (up.button == 3 && count == 1 && xapp->isButton(up.state, Button3Mask)) {
        int no = findItemByPoint(up.x, up.y);

        if (no != -1) {
            YListItem *i = getItem(no);
            if (!isSelected(i)) {
                focusSelectItem(no);
            } else {
                //fFocusedItem = -1;
            }
            YMenu* windowListPopup = windowList->getWindowListPopup();
            enableCommands(windowListPopup);
            windowListPopup->popup(nullptr, nullptr, nullptr,
                                   up.x_root, up.y_root,
                                   YPopupWindow::pfCanFlipVertical |
                                   YPopupWindow::pfCanFlipHorizontal |
                                   YPopupWindow::pfPopupMenu);
        } else {
            YMenu* windowListAllPopup = windowList->getWindowListAllPopup();
            windowListAllPopup->popup(nullptr, nullptr, nullptr,
                                      up.x_root, up.y_root,
                                      YPopupWindow::pfCanFlipVertical |
                                      YPopupWindow::pfCanFlipHorizontal |
                                      YPopupWindow::pfPopupMenu);

        }
        return ;
    }
    YListBox::handleClick(up, count);
}

void WindowListBox::enableCommands(YMenu *popup) {
    bool selected = false;
    bool minified = true;
    bool maxified = true;
    bool horified = true;
    bool vertified = true;
    bool fullscreen = true;
    bool ishidden = true;
    bool rolledup = true;
    bool trayicon = true;
    long workspace = AllWorkspaces;
    bool sameWorkspace = false;
    bool restores = false;
    bool minifies = false;
    bool maxifies = false;
    bool showable = false;
    bool hidable = false;
    bool rollable = false;
    bool raiseable = false;
    bool lowerable = false;
    bool traytoggle = false;
    bool closable = false;

    // enable minimize,hide if appropriate
    // enable workspace selections if appropriate

    popup->enableCommand(actionNull);
    for (IterType iter(getIterator()); ++iter; ) {
        if (isSelected(iter.where())) {
            WindowListItem *item = static_cast<WindowListItem *>(*iter);
            ClientData* frame = item->getFrame();
            if (!frame) {
                continue;
            }
            selected = true;

            fullscreen &= frame->isFullscreen();
            minified &= frame->isMinimized();
            maxified &= frame->isMaximizedFully() && !fullscreen;
            vertified &= frame->isMaximizedVert() && !fullscreen && !maxified;
            horified &= frame->isMaximizedHoriz() && !fullscreen && !maxified;
            ishidden &= frame->isHidden();
            rolledup &= frame->isRollup();
            trayicon &= (frame->getTrayOption() != WinTrayIgnore);

            restores |= (frame->canRestore());
            minifies |= (frame->canMinimize() && !frame->isMinimized());
            maxifies |= (frame->canMaximize());
            showable |= (frame->canShow());
            hidable |= (frame->canHide() && !frame->isHidden());
            rollable |= (frame->canRollup());
            raiseable |= (frame->canRaise());
            lowerable |= (frame->canLower());
            traytoggle |= notbit(frame->frameOptions(), YFrameWindow::foIgnoreTaskBar);
            closable |= (frame->canClose());

            long ws = frame->getWorkspace();
            if (workspace == AllWorkspaces) {
                workspace = ws;
                sameWorkspace = true;
            } else if (workspace != ws) {
                sameWorkspace = false;
            }
            if (frame->isAllWorkspaces())
                sameWorkspace = false;
            frame->updateSubmenus();
        }
    }
    popup->checkCommand(actionMinimize, selected && minified);
    popup->checkCommand(actionMaximize, selected && maxified);
    popup->checkCommand(actionMaximizeVert, selected && vertified);
    popup->checkCommand(actionMaximizeHoriz, selected && horified);
    popup->checkCommand(actionFullscreen, selected && fullscreen);
    popup->checkCommand(actionHide, selected && ishidden);
    popup->checkCommand(actionRollup, selected && rolledup);
    popup->checkCommand(actionToggleTray, selected && trayicon);
    popup->checkCommand(actionOccupyAllOrCurrent,
                        selected && (workspace == AllWorkspaces));
    if (!restores)
        popup->disableCommand(actionRestore);
    if (!minifies)
        popup->disableCommand(actionMinimize);
    if (!maxifies)
        popup->disableCommand(actionMaximize);
    if (!maxifies)
        popup->disableCommand(actionMaximizeVert);
    if (!maxifies)
        popup->disableCommand(actionMaximizeHoriz);
    if (!showable)
        popup->disableCommand(actionShow);
    if (!hidable)
        popup->disableCommand(actionHide);
    if (!rollable)
        popup->disableCommand(actionRollup);
    if (!raiseable)
        popup->disableCommand(actionRaise);
    if (!lowerable)
        popup->disableCommand(actionLower);
    if (!traytoggle)
        popup->disableCommand(actionToggleTray);
    if (!closable)
        popup->disableCommand(actionClose);

    moveMenu->enableCommand(actionNull);
    if (sameWorkspace && workspace != AllWorkspaces) {
        for (int i = 0; i < moveMenu->itemCount(); i++) {
            YMenuItem *item = moveMenu->getItem(i);
            for (int w = 0; w < workspaceCount; w++)
                if (item && item->getAction() == workspaceActionMoveTo[w])
                    item->setEnabled(w != workspace);
        }
    }
    if (selected == false) {
        moveMenu->disableCommand(actionNull);
        popup->disableCommand(actionNull);
    }
    else {
        moveMenu->setActionListener(this);
        layerMenu->setActionListener(this);
    }
}

WindowList::WindowList(YWindow *aParent):
    YFrameClient(aParent, nullptr),
    allWorkspacesItem(nullptr),
    scroll(new YScrollView(this)),
    list(new WindowListBox(scroll, scroll))
{
    addStyle(wsNoExpose);

    if (listbackPixmap != null) {
        scroll->setBackgroundPixmap(listbackPixmap);
    }
    else if (nonempty(clrListBox)) {
        scroll->setBackground(YColorName(clrListBox));
    }

    scroll->setView(list);

    updateWorkspaces();
    setupClient();

    list->show();
    scroll->show();
}

void WindowList::updateWindowListApps() {
    for (YFrameIter frame = manager->focusedIterator(); ++frame; ) {
        frame->addToWindowList();
    }
}

YMenu* WindowList::getWindowListPopup() {
    if (windowListPopup == nullptr) {
        windowListPopup = new WindowListPopup();
        windowListPopup->setActionListener(list);
    }
    return windowListPopup;
}

WindowListPopup::WindowListPopup() {
    addItem(_("_Restore"), -2, KEY_NAME(gKeyWinRestore), actionRestore);
    addItem(_("Mi_nimize"), -2, KEY_NAME(gKeyWinMinimize), actionMinimize);
    addItem(_("Ma_ximize"), -2, KEY_NAME(gKeyWinMaximize), actionMaximize);
    addItem(_("Maximize_Vert"), -2, KEY_NAME(gKeyWinMaximizeVert), actionMaximizeVert);
    addItem(_("MaximizeHori_z"), -2, KEY_NAME(gKeyWinMaximizeHoriz), actionMaximizeHoriz);
    addItem(_("_Fullscreen"), -2, KEY_NAME(gKeyWinFullscreen), actionFullscreen);
    addItem(_("_Show"), -2, null, actionShow);
    addItem(_("_Hide"), -2, KEY_NAME(gKeyWinHide), actionHide);
    addItem(_("Roll_up"), -2, KEY_NAME(gKeyWinRollup), actionRollup);
    addItem(_("_Raise"), -2, KEY_NAME(gKeyWinRaise), actionRaise);
    addItem(_("_Lower"), -2, KEY_NAME(gKeyWinLower), actionLower);
    addSubmenu(_("La_yer"), -2, layerMenu);
    addSeparator();
    addSubmenu(_("Move _To"), -2, moveMenu);
    addItem(_("Occupy _All"), -2, KEY_NAME(gKeyWinOccupyAll), actionOccupyAllOrCurrent);
    addItem(_("Tray _icon"), -2, null, actionToggleTray);
    addSeparator();
    addItem(_("Tile _Vertically"), -2, KEY_NAME(gKeySysTileVertical), actionTileVertical);
    addItem(_("T_ile Horizontally"), -2, KEY_NAME(gKeySysTileHorizontal), actionTileHorizontal);
    addItem(_("Ca_scade"), -2, KEY_NAME(gKeySysCascade), actionCascade);
    addItem(_("_Arrange"), -2, KEY_NAME(gKeySysArrange), actionArrange);
    addSeparator();
    addItem(_("_Minimize All"), -2, KEY_NAME(gKeySysMinimizeAll), actionMinimizeAll);
    addItem(_("_Hide All"), -2, KEY_NAME(gKeySysHideAll), actionHideAll);
    addItem(_("_Undo"), -2, KEY_NAME(gKeySysUndoArrange), actionUndoArrange);
    addSeparator();

    YMenu *closeSubmenu = new YMenu();
    assert(closeSubmenu != 0);

    closeSubmenu->addItem(_("_Close"), -2, _("Del"), actionClose);
    closeSubmenu->addSeparator();
    closeSubmenu->addItem(_("_Kill Client"), -2, null, actionKill);
#if 0
    closeSubmenu->addItem(_("_Terminate Process"), -2, 0, actionTermProcess);
    closeSubmenu->addItem(_("Kill _Process"), -2, 0, actionKillProcess);
#endif

    addItem(_("_Close"), -2, actionClose, closeSubmenu);
}

YMenu* WindowList::getWindowListAllPopup() {
    if (windowListAllPopup == nullptr) {
        windowListAllPopup = new WindowListAllPopup();
        windowListAllPopup->setActionListener(list);
    }
    return windowListAllPopup;
}

WindowListAllPopup::WindowListAllPopup() {
    addItem(_("Tile _Vertically"), -2, KEY_NAME(gKeySysTileVertical), actionTileVertical);
    addItem(_("T_ile Horizontally"), -2, KEY_NAME(gKeySysTileHorizontal), actionTileHorizontal);
    addItem(_("Ca_scade"), -2, KEY_NAME(gKeySysCascade), actionCascade);
    addItem(_("_Arrange"), -2, KEY_NAME(gKeySysArrange), actionArrange);
    addItem(_("_Minimize All"), -2, KEY_NAME(gKeySysMinimizeAll), actionMinimizeAll);
    addItem(_("_Hide All"), -2, KEY_NAME(gKeySysHideAll), actionHideAll);
    addItem(_("_Undo"), -2, KEY_NAME(gKeySysUndoArrange), actionUndoArrange);
}

void WindowList::setupClient() {
    int dx, dy;
    unsigned dw, dh;
    desktop->getScreenGeometry(&dx, &dy, &dw, &dh);

    unsigned w = dw;
    unsigned h = dh;

    setGeometry(YRect(w / 4, h / 4, w / 2, h / 2));

    setTitle("WindowList");
    setWindowTitle(_("Window list"));
    setIconTitle(_("Window list"));
    setClassHint("windowList", "IceWM");
    setNetWindowType(_XA_NET_WM_WINDOW_TYPE_DIALOG);

    setWinHintsHint(WinHintsSkipTaskBar |
                    WinHintsSkipWindowMenu);
    setWorkspaceHint(AllWorkspaces);
    setLayerHint(WinLayerAboveDock);
}

WindowList::~WindowList() {
}

void WindowList::updateWorkspaces() {
    if (workspaceItem.isEmpty()) {
        allWorkspacesItem = new WindowListItem(nullptr, AllWorkspaces);
        workspaceItem.append(allWorkspacesItem);
        list->addItem(allWorkspacesItem);
    }
    for (int ws = workspaceItem.getCount() - 1; ws < workspaceCount; ++ws) {
        workspaceItem.insert(ws, new WindowListItem(nullptr, ws));
        list->addBefore(allWorkspacesItem, workspaceItem[ws]);
    }
    for (int ws = workspaceItem.getCount() - 1; ws > workspaceCount; --ws) {
        list->removeItem(workspaceItem[ws - 1]);
        workspaceItem.remove(ws - 1);
    }
}

void WindowList::handleFocus(const XFocusChangeEvent &focus) {
    if (focus.type == FocusIn && focus.mode != NotifyUngrab) {
        if (width() > 1 && height() > 1 && !getFrame()->isUnmapped()) {
            list->setWindowFocus();
        }
    } else if (focus.type == FocusOut) {
    }
}

void WindowList::relayout() {
    list->repaint();
}

WindowListItem *WindowList::addWindowListApp(YFrameWindow *frame) {
    WindowListItem *item = new WindowListItem(frame, frame->getWorkspace());
    if (item) {
        insertApp(item);
    }
    return item;
}

void WindowList::insertApp(WindowListItem *item) {
    ClientData *frame = item->getFrame();
    if (frame->owner() &&
        frame->owner()->winListItem())
    {
        list->addAfter(frame->owner()->winListItem(), item);
    }
    else {
        int ws = frame->getWorkspace();
        Window lead = frame->clientLeader();
        ClassHint* hint = frame->classHint();
        int start = (0 <= ws && ws + 1 < workspaceItem.getCount())
                    ? list->findItem(workspaceItem[ws])
                    : list->findItem(allWorkspacesItem);
        int stop = (0 <= ws && ws + 1 < workspaceItem.getCount())
                    ? list->findItem(workspaceItem[ws + 1])
                    : list->getItemCount();
        if (0 <= start && start < stop) {
            int best = 1 + start;
            if (lead) {
                int have = 0;
                for (int i = best; i < stop; ++i) {
                    WindowListItem* test =
                        dynamic_cast<WindowListItem*>(list->getItem(i));
                    if (test && test->getFrame() &&
                        lead == test->getFrame()->clientLeader()) {
                        have = i + 1;
                    }
                }
                if (have) {
                    list->insertAt(have, item);
                    return;
                }
            }
            int have = stop;
            for (int i = best; i < stop; ++i) {
                WindowListItem* test =
                    dynamic_cast<WindowListItem*>(list->getItem(i));
                if (test && test->getFrame()) {
                    ClassHint* klas = test->getFrame()->classHint();
                    int cmp =
                        hint == nullptr ? +1 :
                        klas == nullptr ? -1 :
                        nonempty(hint->res_class) ?
                        isEmpty(klas->res_class) ? -1 :
                        strcmp(hint->res_class, klas->res_class) :
                        nonempty(hint->res_name) ?
                        isEmpty(klas->res_name) ? -1 :
                        strcmp(hint->res_name, klas->res_name) : +1;
                    if (cmp < 0) {
                        list->insertAt(i, item);
                        return;
                    }
                    if (cmp == 0) {
                        have = i + 1;
                    }
                }
            }
            list->insertAt(have, item);
        }
        else {
            list->addItem(item);
        }
    }
}

void WindowList::removeWindowListApp(WindowListItem *item) {
    if (item) {
        list->removeItem(item);
        delete item;
    }
}

void WindowList::updateWindowListApp(WindowListItem *item) {
    if (item) {
        list->removeItem(item);
        insertApp(item);
    }
}

void WindowList::repaintItem(WindowListItem *item) {
    list->repaintItem(item);
}

void WindowList::configure(const YRect2& r) {
    if (r.resized()) {
        scroll->setGeometry(YRect(0, 0, r.width(), r.height()));
    }
}

void WindowList::handleClose() {
    if (!getFrame()->isHidden())
        getFrame()->wmHide();
}

void WindowList::showFocused(int x, int y) {
    const YFrameWindow *focus = manager->getFocus();
    if (focus != getFrame() || focus == nullptr) {
        WindowListItem* item = focus ? focus->winListItem()
                             : workspaceItem[manager->activeWorkspace()];
        list->focusSelectItem(list->findItem(item));
    }
    if (getFrame() == nullptr) {
        int scn = desktop->getScreenForRect(x, y, 1, 1);
        YRect geo(desktop->getScreenGeometry(scn));
        long dw = geo.width();
        long dh = long(geo.height() - max(100U, geo.height() / 10));
        long line = list->getLineHeight();
        long need = line * (1L + workspaceCount + manager->focusedCount());
        unsigned w = unsigned(dw / 2);
        unsigned h = unsigned(max(dh / 2, clamp(need, line + 1L, dh)));
        int x = int((dw - w) / 2);
        int y = int((dh - h) / 2);
        setGeometry(YRect(x, y, w, h));
        manager->manageClient(this);
    }
    if (getFrame()) {
        if (x == -1 && y == -1) {
            int ix, iy, iw, ih;
            getFrame()->getNormalGeometryInner(&ix, &iy, &iw, &ih);
            YRect f(ix, iy, iw, ih);
            YRect r(desktop->getScreenGeometry(getFrame()->getScreen()));
            if (r.overlap(f) * 4 < f.pixels()) {
                x = r.xx + r.ww / 2;
                y = r.yy + r.hh / 2;
            }
        }
        if (x != -1 && y != -1) {
            int px, py;

            int xiscreen = desktop->getScreenForRect(x, y, 1, 1);
            int dx, dy;
            unsigned uw, uh;
            desktop->getScreenGeometry(&dx, &dy, &uw, &uh, xiscreen);
            int dw = int(uw), dh = int(uh);

            px = x - int(getFrame()->width() / 2);
            py = y - int(getFrame()->height() / 2);
            if (px + int(getFrame()->width()) > dx + dw)
                px = dx + dw - int(getFrame()->width());
            if (py + int(getFrame()->height()) > dy + dh)
                py = dx + dh - int(getFrame()->height());
            if (px < dx)
                px = dx;
            if (py < dy)
                py = dy;
            getFrame()->setNormalPositionOuter(px, py);
        }
        getFrame()->setRequestedLayer(WinLayerAboveDock);
        getFrame()->setAllWorkspaces();
        getFrame()->activateWindow(true);
    }
}

WindowList* WindowListProxy::acquire() {
    if (wlist == nullptr) {
        wlist = new WindowList(desktop);
        wlist->updateWindowListApps();
    }
    return wlist;
}

// vim: set sw=4 ts=4 et:
