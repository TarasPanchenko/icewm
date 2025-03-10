/*
 * IceWM
 *
 * Copyright (C) 1997-2002 Marko Macek
 */
#include "config.h"
#include "wmframe.h"
#include "wmtitle.h"
#include "wmbutton.h"
#include "wmwinlist.h"
#include "wmapp.h"
#include "wpixmaps.h"
#include "yprefs.h"
#include "prefs.h"
#include "intl.h"

#define ISMASK(w,e,n) (((w) & ~(n)) == (e))

static YFont titleFont;

static YColorName titleBarBackground[2] = {
    &clrInactiveTitleBar, &clrActiveTitleBar
};
static YColorName titleBarForeground[2] = {
    &clrInactiveTitleBarText, &clrActiveTitleBarText
};
static YColorName titleBarShadowText[2] = {
    &clrInactiveTitleBarShadow, &clrActiveTitleBarShadow
};

void YFrameTitleBar::initTitleColorsFonts() {
    if (titleFont == null) {
        titleFont = titleFontName;
    }
}

void freeTitleColorsFonts() {
    titleFont = null;
}

YColor YFrameTitleBar::background(bool active) {
    return titleBarBackground[active];
}

YFrameTitleBar::YFrameTitleBar(YWindow *parent, YFrameWindow *frame):
    YWindow(parent),
    fFrame(frame),
    wasCanRaise(false),
    fVisible(false)
{
    initTitleColorsFonts();
    addStyle(wsNoExpose);
    setWinGravity(NorthGravity);
    setBitGravity(NorthWestGravity);
    addEventMask(VisibilityChangeMask);
    setTitle("TitleBar");

    memset(fButtons, 0, sizeof fButtons);
    relayout();
}

YFrameTitleBar::~YFrameTitleBar() {
    for (auto b : fButtons)
        delete b;
}

bool YFrameTitleBar::isRight(char c) {
    return (strchr(titleButtonsRight, c) != nullptr);
}

bool YFrameTitleBar::supported(char c) {
    return (strchr(titleButtonsSupported, c) != nullptr);
}

YFrameButton *YFrameTitleBar::getButton(char c) {
    unsigned index, flag;
    switch (c) {
    case Depth: index = 1; flag = YFrameWindow::fdDepth;    break;
    case Hide:  index = 2; flag = YFrameWindow::fdHide;     break;
    case Mini:  index = 3; flag = YFrameWindow::fdMinimize; break;
    case Maxi:  index = 4; flag = YFrameWindow::fdMaximize; break;
    case Roll:  index = 5; flag = YFrameWindow::fdRollup;   break;
    case Menu:  index = 6; flag = YFrameWindow::fdSysMenu;  break;
    case Close: index = 7; flag = YFrameWindow::fdClose;    break;
    default:    index = 0; flag = 0; break;
    }
    if (index) {
        if (hasbit(decors(), flag)) {
            if (fButtons[index] == nullptr && supported(c)) {
                fButtons[index] = new YFrameButton(this, c);
            }
        } else if (fButtons[index]) {
            delete fButtons[index];
            fButtons[index] = nullptr;
        }
    }
    return fButtons[index];
}

void YFrameTitleBar::handleButton(const XButtonEvent &button) {
    if (button.type == ButtonPress) {
        if ((buttonRaiseMask & (1 << (button.button - Button1))) &&
           !(button.state & (xapp->AltMask | ControlMask | xapp->ButtonMask))) {
            if ( !getFrame()->isRollup())
                getFrame()->activate();
            wasCanRaise = getFrame()->canRaise();
            if (raiseOnClickTitleBar)
                getFrame()->wmRaise();
        }
    }
    else if (button.type == ButtonRelease) {
        if ((button.button == Button1 || button.button == Button3) &&
            xapp->isButton(button.state, Button1Mask + Button3Mask))
        {
            windowList->showFocused(button.x_root, button.y_root);
        }
    }
    YWindow::handleButton(button);
}

void YFrameTitleBar::handleClick(const XButtonEvent &up, int count) {
    YAction action(actionNull);
    if (count >= 2 && (count % 2 == 0)) {
        if (up.button == titleMaximizeButton &&
            ISMASK(KEY_MODMASK(up.state), 0, ControlMask))
        {
            action = actionMaximize;
        }
        else if (up.button == titleMaximizeButton &&
             ISMASK(KEY_MODMASK(up.state), ShiftMask, ControlMask))
        {
            action = actionMaximizeVert;
        }
        else if (up.button == titleMaximizeButton && xapp->AltMask &&
             ISMASK(KEY_MODMASK(up.state), xapp->AltMask + ShiftMask, ControlMask))
        {
            action = actionMaximizeHoriz;
        }
        else if (up.button == titleRollupButton &&
             up.button <= Button3 &&
             ISMASK(KEY_MODMASK(up.state), 0, ControlMask))
        {
            action = actionRollup;
        }
        else if (up.button == titleRollupButton &&
             up.button <= Button3 &&
             ISMASK(KEY_MODMASK(up.state), ShiftMask, ControlMask))
        {
            action = actionMaximizeHoriz;
        }
    }
    else if (count == 1) {
        if (up.button == Button3 && notbit(KEY_MODMASK(up.state), xapp->AltMask)) {
            getFrame()->popupSystemMenu(this, up.x_root, up.y_root,
                                        YPopupWindow::pfCanFlipVertical |
                                        YPopupWindow::pfCanFlipHorizontal);
        }
        else if (up.button == Button1) {
            if (KEY_MODMASK(up.state) == xapp->AltMask) {
                action = actionLower;
            }
            else if (lowerOnClickWhenRaised &&
                   (buttonRaiseMask & (1 << (up.button - Button1))) &&
                   ((up.state & (ControlMask | xapp->ButtonMask)) ==
                    Button1Mask))
            {
                if (!wasCanRaise) {
                    action = actionLower;
                    wasCanRaise = true;
                }
            }
        }
        else if (inrange<unsigned>(up.button, Button4, Button5) &&
                 inrange<unsigned>(titleRollupButton, Button4, Button5))
        {
            if (up.button == Button4 && !getFrame()->isRollup())
                action = actionRollup;
            if (up.button == Button5 && getFrame()->isRollup())
                action = actionRollup;
        }
    }
    if (action != actionNull) {
        getFrame()->actionPerformed(action, up.state);
    }
}

bool YFrameTitleBar::handleBeginDrag(
        const XButtonEvent &down,
        const XMotionEvent &motion)
{
    // check for a drag on the reparented resize handles
    if (down.subwindow &&
        getFrame()->hasIndicators() &&
        (down.subwindow == getFrame()->topSideIndicator() ||
         down.subwindow == getFrame()->topLeftIndicator() ||
         down.subwindow == getFrame()->topRightIndicator()))
    {
        return getFrame()->handleBeginDrag(down, motion);
    }
    else if (getFrame()->canMove()) {
        getFrame()->startMoveSize(true, true,
                                  0, 0,
                                  down.x + x(), down.y + y());
        return true;
    }
    return false;
}

void YFrameTitleBar::activate() {
    repaint();
    if (LOOK(lookPixmap | lookMetal | lookGtk | lookFlat)) {
        for (auto b : fButtons)
            if (b)
                b->repaint();
    }
    else if (LOOK(lookWin95)) {
        if (menuButton())
            menuButton()->repaint();
    }
}

void YFrameTitleBar::layoutButtons() {
    bool const pi(focused());
    int left(titleJ[pi] != null ? int(titleJ[pi]->width()) : 0);
    int right(int(getFrame()->width()) - 2 * getFrame()->borderX() -
              (titleQ[pi] != null ? int(titleQ[pi]->width()) : 0));

    if (titleButtonsLeft) {
        for (const char *bc = titleButtonsLeft; *bc; bc++) {
            if (*bc == ' ')
                left++;
            else {
                YFrameButton *b = getButton(*bc);
                if (b && b->onRight() == false) {
                    if (left + int(b->width()) >= right) {
                        b->hide();
                    } else {
                        b->setPosition(left, 0);
                        left += b->width();
                        b->show();
                    }
                }
            }
        }
    }

    if (titleButtonsRight) {
        for (const char *bc = titleButtonsRight; *bc; bc++) {
            if (*bc == ' ')
                right--;
            else {
                YFrameButton *b = getButton(*bc);
                if (b && b->onRight()) {
                    if (right - int(b->width()) <= left) {
                        b->hide();
                    } else {
                        right -= b->width();
                        b->setPosition(right, 0);
                        b->show();
                    }
                }
            }
        }
    }
}

void YFrameTitleBar::deactivate() {
    activate(); // for now
}

void YFrameTitleBar::refresh() {
    repaint();
    for (auto b : fButtons)
        if (b)
            b->repaint();
}

void YFrameTitleBar::repaint() {
    if (fVisible && width() > 1 && height() > 1) {
        GraphicsBuffer(this).paint();
    }
}

void YFrameTitleBar::handleVisibility(const XVisibilityEvent& visib) {
    bool prev = fVisible;
    fVisible = (visib.state != VisibilityFullyObscured);
    if (prev < fVisible)
        repaint();
}

void YFrameTitleBar::configure(const YRect2& r) {
}

void YFrameTitleBar::relayout() {
    int h = getFrame()->titleY();
    if (h == 0) {
        hide();
    } else {
        int x = getFrame()->borderX();
        int y = getFrame()->borderY();
        int w = max(1, int(getFrame()->width()) - 2 * x);
        setGeometry(YRect(x, y, w, h));
        layoutButtons();
        repaint();
        show();
    }
}

void YFrameTitleBar::paint(Graphics &g, const YRect &/*r*/) {
    if (getFrame()->client() == nullptr || visible() == false)
        return;

    YColor bg = titleBarBackground[focused()];
    YColor fg = titleBarForeground[focused()];

    int onLeft(0);
    int onRight((int) width());

    if (titleQ[focused()] != null)
        onRight -= int(titleQ[focused()]->width());

    if (titleButtonsLeft) {
        for (const char *bc = titleButtonsLeft; *bc; bc++) {
            if (*bc == ' ')
                ++onLeft;
            else {
                YFrameButton const *b(getButton(*bc));
                if (b && b->visible() && b->onRight() == false)
                    onLeft = max(onLeft, (int)(b->x() + b->width()));
            }
        }
    }

    if (titleButtonsRight) {
        for (const char *bc = titleButtonsRight; *bc; bc++) {
            if (*bc == ' ')
                --onRight;
            else {
                YFrameButton const *b(getButton(*bc));
                if (b && b->visible() && b->onRight())
                    onRight = max(onRight - (int) b->width(), b->x());
            }
        }
    }

    if (titleFont) {
        g.setFont(titleFont);
    }

    mstring title = getFrame()->getTitle();
    int const fontHeight = titleFont ? titleFont->height() : 8;
    int const fontAscent = titleFont ? titleFont->ascent() : 6;
    int const yPos(int(height() - fontHeight) / 2 +
                   fontAscent + titleBarVertOffset);
    int tlen = title != null && titleFont ? titleFont->textWidth(title) : 0;

    int stringOffset(onLeft + (onRight - onLeft - tlen)
                     * (int) titleBarJustify / 100);
    g.setColor(bg);
    switch (wmLook) {
    case lookWin95:
    case lookNice:
        g.fillRect(0, 0, width(), height());
        break;
    case lookWarp3:
        {
            int y = 0;
            int y2 = int(height()) - 1;

            g.fillRect(0, y, width(), height() - 1);
            g.setColor(focused() ? fg.darker() : bg.darker());
            g.drawLine(0, y2, width(), y2);
        }
        break;
    case lookWarp4:
        if (titleBarJustify == 0)
            stringOffset++;
        else if (titleBarJustify == 100)
            stringOffset--;

        if (focused()) {
            g.fillRect(1, 1, width() - 2, height() - 2);
            g.setColor(titleBarBackground[false]);
            g.draw3DRect(onLeft, 0, onRight - 1, height() - 1, false);
        } else {
            g.fillRect(0, 0, width(), height());
        }
        break;
    case lookMotif:
        if (titleBarJustify == 0)
            stringOffset++;
        else if (titleBarJustify == 100)
            stringOffset--;

        g.fillRect(1, 1, width() - 2, height() - 2);
        g.draw3DRect(onLeft, 0, onRight - 1 - onLeft, height() - 1, true);
        break;
    case lookPixmap:
    case lookMetal:
    case lookFlat:
    case lookGtk: {
        bool const pi(focused());

        // !!! we really need a fallback mechanism for small windows
        if (titleL[pi] != null) {
            g.drawPixmap(titleL[pi], onLeft, 0);
            onLeft += int(titleL[pi]->width());
        }

        if (titleR[pi] != null) {
            onRight -= int(titleR[pi]->width());
            g.drawPixmap(titleR[pi], onRight, 0);
        }

        int lLeft(onLeft + (titleP[pi] != null ? (int)titleP[pi]->width() : 0)),
            lRight(onRight - (titleM[pi] != null ? (int)titleM[pi]->width() : 0));

        tlen = clamp(lRight - lLeft, 0, tlen);
        stringOffset = lLeft + (lRight - lLeft - tlen)
            * (int) titleBarJustify / 100;

        lLeft = stringOffset;
        lRight = stringOffset + tlen;

        if (lLeft < lRight) {
            if (rgbTitleT[pi] != null) {
                int const gx(titleBarJoinLeft ? lLeft - onLeft : 0);
                int const gw((titleBarJoinRight ? onRight : lRight) -
                             (titleBarJoinLeft ? onLeft : lLeft));
                g.drawGradient(rgbTitleT[pi], lLeft, 0,
                               lRight - lLeft, height(), gx, 0, gw, height());
            }
            else if (titleT[pi] != null)
                g.repHorz(titleT[pi], lLeft, 0, lRight - lLeft);
            else
                g.fillRect(lLeft, 0, lRight - lLeft, height());
        }

        if (titleP[pi] != null) {
            lLeft -= int(titleP[pi]->width());
            g.drawPixmap(titleP[pi], lLeft, 0);
        }
        if (titleM[pi] != null) {
            g.drawPixmap(titleM[pi], lRight, 0);
            lRight += int(titleM[pi]->width());
        }

        if (onLeft < lLeft) {
            if (rgbTitleS[pi] != null) {
                int const gw((titleBarJoinLeft ? titleBarJoinRight ?
                              onRight : lRight : lLeft) - onLeft);
                g.drawGradient(rgbTitleS[pi], onLeft, 0,
                               lLeft - onLeft, height(), 0, 0, gw, height());
            }
            else if (titleS[pi] != null && titleS[pi]->pixmap())
                g.repHorz(titleS[pi], onLeft, 0, lLeft - onLeft);
            else
                g.fillRect(onLeft, 0, lLeft - onLeft, height());
        }
        if (lRight < onRight) {
            if (rgbTitleB[pi] != null) {
                int const gx(titleBarJoinRight ? titleBarJoinLeft ?
                             lRight - onLeft: lRight - lLeft : 0);
                int const gw(titleBarJoinRight ? titleBarJoinLeft ?
                             onRight - onLeft : onRight - lLeft : onRight - lRight);

                g.drawGradient(rgbTitleB[pi], lRight, 0,
                               onRight - lRight, height(), gx, 0, gw, height());
            }
            else if (titleB[pi] != null && titleB[pi]->pixmap())
                g.repHorz(titleB[pi], lRight, 0, onRight - lRight);
            else
                g.fillRect(lRight, 0, onRight - lRight, height());
        }

        if (titleJ[pi] != null)
            g.drawPixmap(titleJ[pi], 0, 0);
        if (titleQ[pi] != null)
            g.drawPixmap(titleQ[pi], int(width() - titleQ[pi]->width()), 0);

        if (rgbTitleB[pi] != null || rgbTitleS[pi] != null ||
            rgbTitleT[pi] != null)
        {
            g.maxOpacity();
        }
        break;
    }
    }

    if (title != null && tlen && onLeft + 16 < onRight) {
        stringOffset += titleBarHorzOffset;

        if (titleBarShadowText[focused()]) {
            g.setColor(titleBarShadowText[focused()]);
            g.drawStringEllipsis(stringOffset + 1, yPos + 1, title, tlen);
        }

        g.setColor(fg);
        g.drawStringEllipsis(stringOffset, yPos, title, tlen);
    }
}

void YFrameTitleBar::renderShape(Graphics& g) {
#ifdef CONFIG_SHAPE
    if (LOOK(lookPixmap | lookMetal | lookGtk | lookFlat))
    {
        int onLeft(0);
        int onRight((int) width());

        if (titleQ[focused()] != null)
            onRight -= int(titleQ[focused()]->width());

        if (titleButtonsLeft)
            for (const char *bc = titleButtonsLeft; *bc; bc++) {
                if (*bc == ' ')
                    ++onLeft;
                else {
                    YFrameButton const *b(getButton(*bc));
                    if (b && b->visible() && b->onRight() == false) {
                        onLeft = max(onLeft, (int)(b->x() + b->width()));

                        ref<YPixmap> pixmap = b->getPixmap(0);
                        if (pixmap != null && b->getPixmap(1) != null ) {
                            g.copyDrawable(pixmap->mask(), 0, 0,
                                           b->width(),
                                           b->height(),
                                           x() + b->x(),
                                           y() + b->y());
                        }
                    }
                }
            }

        if (titleButtonsRight)
            for (const char *bc = titleButtonsRight; *bc; bc++) {
                if (*bc == ' ')
                    --onRight;
                else {
                    YFrameButton const *b(getButton(*bc));
                    if (b && b->visible() && b->onRight()) {
                        onRight = max(onRight - (int) b->width(), b->x());

                        ref<YPixmap> pixmap = b->getPixmap(0);
                        if ( pixmap != null && b->getPixmap(1) != null ) {
                            g.copyDrawable(pixmap->mask(), 0, 0,
                                           b->width(),
                                           b->height(),
                                           x() + b->x(),
                                           y() + b->y());
                        }
                    }
                }
            }

        onLeft += x();
        onRight += x();

        mstring title = getFrame()->getTitle();
        int tlen = title != null && titleFont ? titleFont->textWidth(title) : 0;
        bool const pi(focused());

        if (titleL[pi] != null) {
            g.drawMask(titleL[pi], onLeft, y());
            onLeft += int(titleL[pi]->width());
        }

        if (titleR[pi] != null) {
            onRight -= int(titleR[pi]->width());
            g.drawMask(titleR[pi], onRight, y());
        }

        int lLeft(onLeft + (titleP[pi] != null ? (int)titleP[pi]->width() : 0)),
            lRight(onRight - (titleM[pi] != null ? (int)titleM[pi]->width() : 0));

        tlen = clamp(lRight - lLeft, 0, tlen);
        int stringOffset = lLeft + (lRight - lLeft - tlen)
                                 * (int) titleBarJustify / 100;

        lLeft = stringOffset;
        lRight = stringOffset + tlen;

        if (lLeft < lRight && titleT[pi] != null)
            g.repHorz(titleT[pi]->mask(),
                      titleT[pi]->width(), titleT[pi]->height(),
                      lLeft, y(), lRight - lLeft);

        if (titleP[pi] != null) {
            lLeft -= int(titleP[pi]->width());
            g.drawMask(titleP[pi], lLeft, y());
        }
        if (titleM[pi] != null) {
            g.drawMask(titleM[pi], lRight, y());
            lRight += int(titleM[pi]->width());
        }

        if (onLeft < lLeft && titleS[pi] != null)
            g.repHorz(titleS[pi]->mask(),
                      titleS[pi]->width(), titleS[pi]->height(),
                      onLeft, y(), lLeft - onLeft);

        if (lRight < onRight && titleB[pi] != null)
            g.repHorz(titleB[pi]->mask(),
                      titleB[pi]->width(), titleB[pi]->height(),
                      lRight, y(), onRight - lRight);

        if (titleJ[pi] != null)
            g.drawMask(titleJ[pi], x(), y());
        if (titleQ[pi] != null)
            g.drawMask(titleQ[pi], int(x() + width() - titleQ[pi]->width()), y());
    }
#endif
}

// vim: set sw=4 ts=4 et:
