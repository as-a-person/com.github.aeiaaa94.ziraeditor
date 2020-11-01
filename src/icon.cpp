#include "icon.h"
#include <QFileInfo>
#include <QPainter>
#include "helper.h"

const int ICON_SIZE = 64;

/*
Action names:
    actionOpenFile
    actionNewFile
    actionNewFolder
    actionNewProject
    actionOpenProject
    actionUpdateProject
    actionRescanProject
    actionEditProject
    actionSaveAll
    actionSave
    actionSaveAs
    actionClose
    actionCloseProject
    actionQuit
    actionNewFile
    actionNewFolder
    actionNewProject
    actionUndo
    actionRedo
    actionBack
    actionForward
    actionFindReplace
    actionSearchInFiles
    actionSettings
    actionGitStatus
    actionGitLog
    actionGitDiffTree
    actionGitDiffAll
    actionGitDiffCurrent
    actionGitDiffAllCommit
    actionGitDiffCurrentCommit
    actionGitDiscardChanges
    actionGitCancelCommit
    actionGitDiscardCommit
    actionGitRevert
    actionGitAddAll
    actionGitAddCurrent
    actionGitResetAll
    actionGitResetCurrent
    actionGitCommit
    actionGitPush
    actionGitPull
    actionServersStatus
    actionStartServers
    actionStopServers
    actionColorPicker
    actionCompileSass
    actionExecuteFile
    actionExecuteSelection
    actionSplitTab
    actionOpenContextMenu
    actionShowHideSidebar
    actionShowHideToolbar
    actionShowHideOutput
    actionDisplayDockButtons
    actionQuickAccess
    actionHelpContact
    actionHelpDonate
    actionHelpZiraCMS
    actionHelpShortcuts
    actionHelpFAQ
    actionHelpAbout
Other names:
    actionSidebar
    actionOutput
    actionEdit
    actionDelete
    actionRefresh
    actionCopy
    actionPaste
    actionCut
    actionOpenDeclaration
    actionGoto
    actionHelp
    actionTabsList
    file
    folder
    left
    right
    up
    down
    clear
    separator
*/

QString Icon::prefix = "";

Icon::Icon()
{

}

void Icon::reset()
{
    Icon::prefix = "";
}

void Icon::iterateMenuActions(QMenu * menu)
{
    QList<QAction *>actions = menu->actions();
    for (QAction * action : actions) {
        if (action->isSeparator()) continue;
        QString actionName = action->objectName();
        QIcon icon = get(actionName);
        if (!icon.isNull()) action->setIcon(icon);
        QMenu * childMenu = action->menu();
        if (childMenu != nullptr) {
            iterateMenuActions(childMenu);
        }
    }
}

void Icon::applyActionIcons(QMenuBar * menuBar, QString prefix)
{
    Icon::prefix = prefix;
    QList<QMenu *>topMenus = menuBar->findChildren<QMenu *>();
    for (QMenu * topMenu : topMenus) {
        iterateMenuActions(topMenu);
    }
}

QIcon Icon::get(QString actionName, QIcon defaultIcon)
{
    if (actionName.size() == 0) return defaultIcon;
    QString path = prefix + "/" + actionName + ".png";
    if (!QFileInfo::exists(path)) return defaultIcon;
    QIcon icon(path);
    if (icon.isNull()) return defaultIcon;
    // disabled icon
    QImage img(ICON_SIZE, ICON_SIZE, QImage::Format_ARGB32);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.drawPixmap(img.rect(), QPixmap(path));
    p.fillRect(img.rect(), QColor(128, 128, 128));
    icon.addPixmap(QPixmap::fromImage(img), QIcon::Disabled);
    return icon;
}
