/*******************************************
 * Zira Editor
 * A lightweight PHP Editor
 * (C)2019 https://github.com/ziracms/editor
 *******************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "helper.h"
#include "settings.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDirIterator>
#include <QMimeData>
#include <QShortcut>
#include <QTimer>
#include <QTextCursor>
#include <QColorDialog>
#include <QInputDialog>
#include <QTextStream>
#include <QDesktopServices>
#include <QPluginLoader>
#include <QStyleFactory>
#include <QStylePlugin>
#include <QVersionNumber>
#include <QScreen>
#include "editortab.h"
#include "searchdialog.h"
#include "servers.h"
#include "settingsdialog.h"
#include "helpdialog.h"
#include "docktitlebar.h"
#include "icon.h"
#include "colordialog.h"
#include "spellchecker.h"
#include "terminal.h"
#include "shortcutsdialog.h"

const int OUTPUT_TAB_MESSAGES_INDEX = 0;
const int OUTPUT_TAB_HELP_INDEX = 1;
const int OUTPUT_TAB_SEARCH_INDEX = 2;
const int OUTPUT_TAB_RESULTS_INDEX = 3;
//const int OUTPUT_TAB_TODO_INDEX = 4;

const int SIDEBAR_TAB_FILE_BROWSER_INDEX = 0;
const int SIDEBAR_TAB_NAVIGATOR_INDEX = 1;
const int SIDEBAR_TAB_GIT_BROWSER_INDEX = 2;

bool MainWindow::WANT_RESTART = false;

int const TERMINAL_START_DELAY = 250; // should not be less then PROJECT_LOAD_DELAY
int const CHECK_SCALE_FACTOR_DELAY = 2000;
int const INPUT_METHOD_ENSURE_CURSOR_VISIBLE_DELAY = 500;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qRegisterMetaType<ParsePHP::ParseResult>();
    qRegisterMetaType<ParseJS::ParseResult>();
    qRegisterMetaType<ParseCSS::ParseResult>();
    qRegisterMetaType<WordsMapList>();

    Settings::load();
    connect(&Settings::instance(), SIGNAL(restartApp()), this, SLOT(restartApp()));

    // loading built-in fonts
    QFontDatabase::addApplicationFont(":/fonts/SourceCodePro-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/SourceCodePro-Bold.ttf");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/RobotoMono-Bold.ttf");
    QFontDatabase::addApplicationFont(":/fonts/UbuntuMono-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/UbuntuMono-Bold.ttf");

    // app font
    QFont appFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    std::string appFontFamily = Settings::get("app_font_family");
    std::string appFontSize = Settings::get("app_font_size");
    if (appFontFamily.size() > 0) {
        appFont.setFamily(QString::fromStdString(appFontFamily));
        appFont.setStyleHint(QFont::SansSerif);
    }
    appFont.setPointSize(std::stoi(appFontSize));
    appFont.setStyleName("");
    QApplication::setFont(appFont);

    QString pluginsDir = QString::fromStdString(Settings::get("plugins_path"));

    theme = QString::fromStdString(Settings::get("theme"));
    colorSheme = QString::fromStdString(Settings::get("color_scheme"));
    customThemesPath = QString::fromStdString(Settings::get("custom_themes_path"));
    if (customThemesPath.size() == 0) {
        QDir customThemesPathDir = QDir("./"+CUSTOM_THEMES_FALLBACK_FOLDER);
        customThemesPath = customThemesPathDir.absolutePath();
        if (!Helper::folderExists(customThemesPath)) customThemesPath = "";
    }
    if (colorSheme == COLOR_SCHEME_DARK) Settings::applyDarkColors();
    else if (colorSheme == COLOR_SCHEME_LIGHT || customThemesPath.size() == 0 || !Helper::fileExists(customThemesPath + "/" + colorSheme + "/" + CUSTOM_THEME_COLORS_FILE)) Settings::applyLightColors();
    else if (customThemesPath.size() > 0 && Helper::fileExists(customThemesPath + "/" + colorSheme + "/" + CUSTOM_THEME_COLORS_FILE)) Settings::applyCustomColors(customThemesPath + "/" + colorSheme + "/" + CUSTOM_THEME_COLORS_FILE);

    QString schemeType = QString::fromStdString(Settings::get(COLOR_SCHEME_TYPE.toStdString()));
    if (theme != THEME_SYSTEM && theme.indexOf(STYLE_PLUGIN_DISPLAY_NAME_SUFFIX) < 0) {
        Style * style = new Style(schemeType == COLOR_SCHEME_LIGHT);
        QApplication::setStyle(style);
        QApplication::setPalette(style->standardPalette());
    }

    // font issue workaround for Qt < 5.12
    bool applyWidgetsFont = false;
    if (Helper::isQtVersionLessThan(5, 12, 0)) applyWidgetsFont = true;

    #if defined(Q_OS_ANDROID)
    applyWidgetsFont = true;
    #endif

    // styles
    applyThemeColors(pluginsDir, schemeType == COLOR_SCHEME_LIGHT, applyWidgetsFont && theme != THEME_SYSTEM && theme.indexOf(STYLE_PLUGIN_DISPLAY_NAME_SUFFIX) < 0);

    ui->setupUi(this);
    setAcceptDrops(true);

    applyThemeIcons();

    if (applyWidgetsFont && theme != THEME_SYSTEM && theme.indexOf(STYLE_PLUGIN_DISPLAY_NAME_SUFFIX) > 0) {
        ui->tabWidget->setFont(appFont);
        ui->sidebarTabWidget->setFont(appFont);
        ui->outputTabWidget->setFont(appFont);
    }

    // setting main menu font
    ui->menuBar->setFont(appFont);
    QList<QMenu *> submenus = ui->menuBar->findChildren<QMenu *>();
    for (auto submenu : submenus) {
        submenu->setFont(appFont);
    }

    #if defined(Q_OS_ANDROID)
    ui->menuBar->setNativeMenuBar(false);
    #endif

    disableActionsForEmptyTabs();
    disableActionsForEmptyProject();

    // restore window geometry & state
    QSettings windowSettings;
    if (windowSettings.allKeys().size() > 0) {
        restoreGeometry(windowSettings.value("main_window_geometry").toByteArray());
        restoreState(windowSettings.value("main_window_state").toByteArray());
    } else {
        ui->sidebarDockWidget->hide();
        ui->outputDockWidget->hide();
    }
    // plugins
    SpellChecker::instance().load();
    terminal = Terminal::instance().load();

    HighlightWords::setColors();

    // load words
    HighlightWords::loadDelayed();
    CompleteWords::loadDelayed();
    HelpWords::loadDelayed();
    SpellWords::loadDelayed();
    Snippets::load();

    // welcome screen
    welcomeScreen = new Welcome(schemeType == COLOR_SCHEME_LIGHT);
    ui->centralWidget->layout()->addWidget(welcomeScreen);
    connect(welcomeScreen, SIGNAL(openProject()), this, SLOT(on_actionOpenProject_triggered()));
    connect(welcomeScreen, SIGNAL(createProject()), this, SLOT(on_actionNewProject_triggered()));

    // editor tabs
    editorTabs = new EditorTabs(ui->tabWidget);
    connect(editorTabs, SIGNAL(statusBarText(QString)), this, SLOT(setStatusBarText(QString)));
    connect(editorTabs, SIGNAL(editorFilenameChanged(QString)), this, SLOT(editorFilenameChanged(QString)));
    connect(editorTabs, SIGNAL(tabOpened(int)), this, SLOT(editorTabOpened(int)));
    connect(editorTabs, SIGNAL(tabSwitched(int)), this, SLOT(editorTabSwitched(int)));
    connect(editorTabs, SIGNAL(tabClosed(int)), this, SLOT(editorTabClosed(int)));
    connect(editorTabs, SIGNAL(modifiedStateChanged(bool)), this, SLOT(editorModifiedStateChanged(bool)));
    connect(editorTabs, SIGNAL(editorSaved(int)), this, SLOT(editorSaved(int)));
    connect(editorTabs, SIGNAL(editorReady(int)), this, SLOT(editorReady(int)));
    connect(editorTabs, SIGNAL(editorShowDeclaration(QString)), this, SLOT(editorShowDeclaration(QString)));
    connect(editorTabs, SIGNAL(editorShowHelp(QString)), this, SLOT(editorShowHelp(QString)));
    connect(editorTabs, SIGNAL(editorParsePHPRequested(int,QString)), this, SLOT(editorParsePHPRequested(int,QString)));
    connect(editorTabs, SIGNAL(editorParseJSRequested(int,QString)), this, SLOT(editorParseJSRequested(int,QString)));
    connect(editorTabs, SIGNAL(editorParseCSSRequested(int,QString)), this, SLOT(editorParseCSSRequested(int,QString)));
    connect(editorTabs, SIGNAL(editorUndoRedoChanged()), this, SLOT(editorUndoRedoChanged()));
    connect(editorTabs, SIGNAL(editorBackForwardChanged()), this, SLOT(editorBackForwardChanged()));
    connect(editorTabs, SIGNAL(editorSearchInFilesRequested(QString)), this, SLOT(editorSearchInFilesRequested(QString)));
    connect(editorTabs, SIGNAL(updateProject()), this, SLOT(on_actionUpdateProject_triggered()));
    connect(editorTabs, SIGNAL(editorFocused()), this, SLOT(editorFocused()));
    connect(editorTabs, SIGNAL(editorBreadcrumbsClick()), this, SLOT(on_actionQuickAccess_triggered()));
    connect(editorTabs, SIGNAL(editorShowPopupTextRequested(QString)), this, SLOT(showPopupText(QString)));
    connect(editorTabs, SIGNAL(editorShowPopupErrorRequested(QString)), this, SLOT(showPopupError(QString)));
    connect(editorTabs, SIGNAL(gitTabRefreshRequested()), this, SLOT(gitTabRefreshRequested()));
    connect(editorTabs, SIGNAL(editorTabsResize()), this, SLOT(editorTabsResize()));
    //connect(editorTabs, SIGNAL(editorPaneResize()), this, SLOT(editorPaneResize()));

    ui->tabWidget->tabBar()->setExpanding(false);
    ui->sidebarTabWidget->tabBar()->setExpanding(false);
    ui->outputTabWidget->tabBar()->setExpanding(false);

    // tab list
    tabsListButton = new QToolButton(ui->tabWidget);
    tabsListButton->setIcon(Icon::get("actionTabsList", QIcon(":/icons/leveldown.png")));
    tabsListButton->setToolTip(tr("Tabs list"));
    tabsListButton->hide();

    tabsList = new TabsList(this);
    connect(tabsListButton, SIGNAL(clicked()), this, SLOT(tabsListTriggered()));
    connect(tabsList, SIGNAL(itemClicked(int)), this, SLOT(tabsListSelected(int)));

    // split tabs
    tabWidgetSplit = new QTabWidget(this);
    tabWidgetSplit->setTabsClosable(true);
    tabWidgetSplit->setMovable(true);

    editorsSplitter = new QSplitter(this);
    ui->centralWidget->layout()->addWidget(editorsSplitter);
    editorsSplitter->addWidget(ui->tabWidget);
    editorsSplitter->addWidget(tabWidgetSplit);

    editorTabsSplit = new EditorTabs(tabWidgetSplit);
    tabWidgetSplit->hide();
    isSplitActive = false;

    connect(editorTabsSplit, SIGNAL(statusBarText(QString)), this, SLOT(setStatusBarText(QString)));
    connect(editorTabsSplit, SIGNAL(editorFilenameChanged(QString)), this, SLOT(editorFilenameChanged(QString)));
    connect(editorTabsSplit, SIGNAL(tabOpened(int)), this, SLOT(editorTabSplitOpened(int)));
    connect(editorTabsSplit, SIGNAL(tabSwitched(int)), this, SLOT(editorTabSplitSwitched(int)));
    connect(editorTabsSplit, SIGNAL(tabClosed(int)), this, SLOT(editorTabSplitClosed(int)));
    connect(editorTabsSplit, SIGNAL(modifiedStateChanged(bool)), this, SLOT(editorModifiedStateChanged(bool)));
    connect(editorTabsSplit, SIGNAL(editorSaved(int)), this, SLOT(editorSplitSaved(int)));
    connect(editorTabsSplit, SIGNAL(editorReady(int)), this, SLOT(editorSplitReady(int)));
    connect(editorTabsSplit, SIGNAL(editorShowDeclaration(QString)), this, SLOT(editorShowDeclaration(QString)));
    connect(editorTabsSplit, SIGNAL(editorShowHelp(QString)), this, SLOT(editorShowHelp(QString)));
    connect(editorTabsSplit, SIGNAL(editorUndoRedoChanged()), this, SLOT(editorUndoRedoChanged()));
    connect(editorTabsSplit, SIGNAL(editorBackForwardChanged()), this, SLOT(editorBackForwardChanged()));
    connect(editorTabsSplit, SIGNAL(editorSearchInFilesRequested(QString)), this, SLOT(editorSearchInFilesRequested(QString)));
    connect(editorTabsSplit, SIGNAL(updateProject()), this, SLOT(on_actionUpdateProject_triggered()));
    connect(editorTabsSplit, SIGNAL(editorFocused()), this, SLOT(editorSplitFocused()));
    connect(editorTabsSplit, SIGNAL(editorBreadcrumbsClick()), this, SLOT(on_actionQuickAccess_triggered()));
    connect(editorTabsSplit, SIGNAL(editorShowPopupTextRequested(QString)), this, SLOT(showPopupText(QString)));
    connect(editorTabsSplit, SIGNAL(editorShowPopupErrorRequested(QString)), this, SLOT(showPopupError(QString)));
    connect(editorTabsSplit, SIGNAL(gitTabRefreshRequested()), this, SLOT(gitTabRefreshRequested()));

    // filebrowser
    filebrowser = new FileBrowser(ui->fileBrowserTreeWidget, ui->fileBrowserPathLine);
    connect(filebrowser, SIGNAL(openFile(QString)), this, SLOT(fileBrowserOpen(QString)));
    connect(filebrowser, SIGNAL(fileCreated(QString)), editorTabs, SLOT(fileBrowserCreated(QString)));
    connect(filebrowser, SIGNAL(fileOrFolderRenamed(QString, QString)), editorTabs, SLOT(fileBrowserRenamed(QString, QString)));
    connect(filebrowser, SIGNAL(fileOrFolderRenamed(QString, QString)), editorTabsSplit, SLOT(fileBrowserRenamed(QString, QString)));
    connect(filebrowser, SIGNAL(fileDeleted(QString)), editorTabs, SLOT(fileBrowserDeleted(QString)));
    connect(filebrowser, SIGNAL(projectCreateRequested(QString,QString, bool, bool)), this, SLOT(projectCreateRequested(QString, QString, bool, bool)));
    connect(filebrowser, SIGNAL(projectEditRequested(QString,QString, bool, bool)), this, SLOT(projectEditRequested(QString, QString, bool, bool)));
    connect(filebrowser, SIGNAL(projectOpenRequested(QString)), this, SLOT(projectOpenRequested(QString)));
    connect(filebrowser, SIGNAL(showMessage(QString)), this, SLOT(showPopupText(QString)));
    connect(filebrowser, SIGNAL(showError(QString)), this, SLOT(showPopupError(QString)));

    // navigator
    navigator = new Navigator(ui->navigatorTreeWidget);
    connect(navigator, SIGNAL(showLine(int)), this, SLOT(editorShowLine(int)));

    // hide sidebar progressbar
    ui->sidebarProgressBarWrapperWidget->setVisible(false);

    // project class
    project = &Project::instance();
    project->init();
    connect(project, SIGNAL(openTabsRequested(QStringList, bool)), this, SLOT(openTabsRequested(QStringList, bool)));
    connect(project, SIGNAL(gotoTabLinesRequested(QList<int>)), this, SLOT(gotoTabLinesRequested(QList<int>)));
    connect(project, SIGNAL(switchToTabRequested(int)), this, SLOT(switchToTabRequested(int)));
    connect(project, SIGNAL(closeAllTabsRequested()), this, SLOT(closeAllTabsRequested()));
    connect(project, SIGNAL(showTodoRequested(QString)), this, SLOT(showTodoRequested(QString)));

    // git
    git = &Git::instance();
    git->init();
    connect(git, SIGNAL(runGitCommand(QString,QString,QStringList,bool,bool)), this, SLOT(runGitCommand(QString,QString,QStringList,bool,bool)));

    gitBrowser = new GitBrowser(ui->gitTabTreeWidget);
    connect(ui->gitTabPullButton, SIGNAL(pressed()), this, SLOT(on_actionGitPull_triggered()));
    connect(ui->gitTabPushButton, SIGNAL(pressed()), this, SLOT(on_actionGitPush_triggered()));
    connect(ui->gitTabRefreshButton, SIGNAL(pressed()), this, SLOT(gitTabRefreshRequested()));
    connect(ui->gitTabCommitButton, SIGNAL(pressed()), this, SLOT(gitTabAddAndCommitRequested()));
    connect(gitBrowser, SIGNAL(addRequested(QString)), this, SLOT(gitTabAddRequested(QString)));
    connect(gitBrowser, SIGNAL(resetRequested(QString)), this, SLOT(gitTabResetRequested(QString)));
    connect(gitBrowser, SIGNAL(commitRequested()), this, SLOT(on_actionGitCommit_triggered()));

    // quick access widget
    qa = new QuickAccess(this);
    connect(qa, SIGNAL(quickAccessRequested(QString,int)), this, SLOT(quickAccessRequested(QString,int)));
    connect(qa, SIGNAL(quickFindRequested(QString)), this, SLOT(quickFindRequested(QString)));

    // messages popup
    popup = new Popup(this);

    // progress line
    progressLine = new ProgressLine(this);
    progressInfo = new ProgressInfo(this);

    // enable php lint & cs
    parsePHPLintEnabled = false;
    std::string parsePHPLintEnabledStr = Settings::get("parser_enable_php_lint");
    if (parsePHPLintEnabledStr == "yes") parsePHPLintEnabled = true;
    parsePHPCSEnabled = false;
    std::string parsePHPCSEnabledStr = Settings::get("parser_enable_php_cs");
    if (parsePHPCSEnabledStr == "yes") parsePHPCSEnabled = true;
    parsePHPEnabled = false;
    std::string parsePHPEnabledStr = Settings::get("parser_enable_parse_php");
    if (parsePHPEnabledStr == "yes") parsePHPEnabled = true;
    parseJSEnabled = false;
    std::string parseJSEnabledStr = Settings::get("parser_enable_parse_js");
    if (parseJSEnabledStr == "yes") parseJSEnabled = true;
    parseCSSEnabled = false;
    std::string parseCSSEnabledStr = Settings::get("parser_enable_parse_css");
    if (parseCSSEnabledStr == "yes") parseCSSEnabled = true;
    gitCommandsEnabled = false;
    std::string gitCommandsEnabledStr = Settings::get("parser_enable_git");
    if (gitCommandsEnabledStr == "yes") gitCommandsEnabled = true;
    serverCommandsEnabled = false;
    std::string serverCommandsEnabledStr = Settings::get("parser_enable_servers");
    if (serverCommandsEnabledStr == "yes") serverCommandsEnabled = true;

    // disable server commands on Android
    #if defined(Q_OS_ANDROID)
    serverCommandsEnabled = false;
    #endif

    // parser
    parserWorker = new ParserWorker();
    parserWorker->moveToThread(&parserThread);
    connect(&parserThread, &QThread::finished, parserWorker, &QObject::deleteLater);
    connect(this, SIGNAL(initWorker()), parserWorker, SLOT(init()));
    connect(this, SIGNAL(disableWorker()), parserWorker, SLOT(disable()));
    connect(this, SIGNAL(parseLint(int,QString)), parserWorker, SLOT(lint(int,QString)));
    connect(this, SIGNAL(execPHP(int,QString)), parserWorker, SLOT(execPHP(int,QString)));
    connect(this, SIGNAL(execSelection(int,QString)), parserWorker, SLOT(execSelection(int,QString)));
    connect(this, SIGNAL(startPHPWebServer(QString)), parserWorker, SLOT(startPHPWebServer(QString)));
    connect(this, SIGNAL(stopPHPWebServer()), parserWorker, SLOT(stopPHPWebServer()));
    connect(this, SIGNAL(parsePHPCS(int,QString)), parserWorker, SLOT(phpcs(int,QString)));
    connect(this, SIGNAL(parseMixed(int,QString)), parserWorker, SLOT(parseMixed(int,QString)));
    connect(this, SIGNAL(parseJS(int,QString)), parserWorker, SLOT(parseJS(int,QString)));
    connect(this, SIGNAL(parseCSS(int,QString)), parserWorker, SLOT(parseCSS(int,QString)));
    connect(this, SIGNAL(parseProject(QString)), parserWorker, SLOT(parseProject(QString)));
    connect(this, SIGNAL(searchInFiles(QString,QString,QString,bool,bool,bool,QStringList)), parserWorker, SLOT(searchInFiles(QString,QString,QString,bool,bool,bool,QStringList)));
    connect(this, SIGNAL(gitCommand(QString, QString, QStringList, bool, bool)), parserWorker, SLOT(gitCommand(QString, QString, QStringList, bool, bool)));
    connect(this, SIGNAL(serversCommand(QString, QString)), parserWorker, SLOT(serversCommand(QString,QString)));
    connect(this, SIGNAL(sassCommand(QString, QString)), parserWorker, SLOT(sassCommand(QString,QString)));
    connect(this, SIGNAL(quickFind(QString, QString, WordsMapList, QStringList)), parserWorker, SLOT(quickFind(QString, QString, WordsMapList, QStringList)));
    connect(this, SIGNAL(installAndroidPack()), parserWorker, SLOT(installAndroidPack()));
    connect(progressInfo, SIGNAL(cancelTriggered()), parserWorker, SLOT(cancelRequested()));
    connect(parserWorker, SIGNAL(lintFinished(int,QStringList,QStringList,QString)), this, SLOT(parseLintFinished(int,QStringList,QStringList,QString)));
    connect(parserWorker, SIGNAL(execPHPFinished(int,QString)), this, SLOT(execPHPFinished(int,QString)));
    connect(parserWorker, SIGNAL(execPHPWebServerFinished(bool,QString)), this, SLOT(execPHPWebServerFinished(bool,QString)));
    connect(parserWorker, SIGNAL(phpcsFinished(int,QStringList,QStringList)), this, SLOT(parsePHPCSFinished(int,QStringList,QStringList)));
    connect(parserWorker, SIGNAL(parseMixedFinished(int,ParsePHP::ParseResult)), this, SLOT(parseMixedFinished(int,ParsePHP::ParseResult)));
    connect(parserWorker, SIGNAL(parseJSFinished(int,ParseJS::ParseResult)), this, SLOT(parseJSFinished(int,ParseJS::ParseResult)));
    connect(parserWorker, SIGNAL(parseCSSFinished(int,ParseCSS::ParseResult)), this, SLOT(parseCSSFinished(int,ParseCSS::ParseResult)));
    connect(parserWorker, SIGNAL(parseProjectFinished(bool,bool)), this, SLOT(parseProjectFinished(bool,bool)));
    connect(parserWorker, SIGNAL(parseProjectProgress(int)), this, SLOT(sidebarProgressChanged(int)));
    connect(parserWorker, SIGNAL(searchInFilesFound(QString,QString,int,int)), this, SLOT(searchInFilesFound(QString,QString,int,int)));
    connect(parserWorker, SIGNAL(searchInFilesFinished()), this, SLOT(searchInFilesFinished()));
    connect(parserWorker, SIGNAL(message(QString)), this, SLOT(workerMessage(QString)));
    connect(parserWorker, SIGNAL(gitCommandFinished(QString,QString,bool)), this, SLOT(gitCommandFinished(QString,QString,bool)));
    connect(parserWorker, SIGNAL(serversCommandFinished(QString)), this, SLOT(serversCommandFinished(QString)));
    connect(parserWorker, SIGNAL(sassCommandFinished(QString,QString)), this, SLOT(sassCommandFinished(QString,QString)));
    connect(parserWorker, SIGNAL(quickFound(QString,QString,QString,int)), qa, SLOT(quickFound(QString,QString,QString,int)));
    connect(parserWorker, SIGNAL(activateProgress()), this, SLOT(activateProgressLine()));
    connect(parserWorker, SIGNAL(deactivateProgress()), this, SLOT(deactivateProgressLine()));
    connect(parserWorker, SIGNAL(activateProgressInfo(QString)), this, SLOT(activateProgressInfo(QString)));
    connect(parserWorker, SIGNAL(deactivateProgressInfo()), this, SLOT(deactivateProgressInfo()));
    connect(parserWorker, SIGNAL(updateProgressInfo(QString)), this, SLOT(updateProgressInfo(QString)));
    connect(parserWorker, SIGNAL(installAndroidPackFinished(QString)), this, SLOT(installAndroidPackFinished(QString)));
    parserThread.start();

    tmpDisableParser = false;

    // message templates
    QString outputMsgErrorColor = QString::fromStdString(Settings::get("messages_error_color"));
    QString outputMsgWarningColor = QString::fromStdString(Settings::get("messages_warning_color"));
    outputMsgErrorTpl = "<p style=\"color:"+outputMsgErrorColor+"\"><a href=\"%1\">["+tr("Line")+": %1]</a> %2</p>";
    outputMsgWarningTpl = "<p style=\"color:"+outputMsgWarningColor+"\"><a href=\"%1\">["+tr("Line")+": %1]</a> %2</p>";

    outputMsgCount = 0;
    ui->messagesBrowser->setOpenLinks(false);
    ui->messagesBrowser->setOpenExternalLinks(false);
    ui->messagesBrowser->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    connect(ui->messagesBrowser, SIGNAL(anchorClicked(QUrl)), this, SLOT(messagesBrowserAnchorClicked(QUrl)));

    phpManualHeaderExpr = QRegularExpression("<div [^>]*class=\"navbar navbar-fixed-top\"[^>]*>[\\s]*<div[^>]*>.+?</div>[\\s]*</div>", QRegularExpression::DotMatchesEverythingOption);
    phpManualBreadcrumbsExpr = QRegularExpression("<div [^>]*id=\"breadcrumbs\"[^>]*>.+?</div>", QRegularExpression::DotMatchesEverythingOption);

    ui->helpBrowser->setOpenLinks(false);
    ui->helpBrowser->setOpenExternalLinks(false);
    ui->helpBrowser->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    connect(ui->helpBrowser, SIGNAL(anchorClicked(QUrl)), this, SLOT(helpBrowserAnchorClicked(QUrl)));

    resetLastSearchParams();
    connect(ui->searchListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(searchListItemDoubleClicked(QListWidgetItem*)));

    // todo tab is disabled by default
    ui->todoTab->setEnabled(false);

    connect(ui->menuEdit, SIGNAL(aboutToShow()), this, SLOT(menuEditOnShow()));
    connect(ui->menuView, SIGNAL(aboutToShow()), this, SLOT(menuViewOnShow()));
    connect(ui->menuTools, SIGNAL(aboutToShow()), this, SLOT(menuToolsOnShow()));

    /*
    if (ui->mainToolBar->orientation() == Qt::Vertical) {
        ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    } else {
        ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    }
    */
    //connect(ui->mainToolBar, SIGNAL(orientationChanged(Qt::Orientation)), this, SLOT(toolbarOrientationChanged(Qt::Orientation)));

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainToolBar->addWidget(spacer);
    QAction * sidebarAction = ui->mainToolBar->addAction(Icon::get("actionSidebar", QIcon(":icons/sidebar.png")), tr("Sidebar"));
    QAction * outputAction = ui->mainToolBar->addAction(Icon::get("actionOutput", QIcon(":icons/output.png")), tr("Output"));
    connect(sidebarAction, SIGNAL(triggered(bool)), this, SLOT(sidebarActionTriggered(bool)));
    connect(outputAction, SIGNAL(triggered(bool)), this, SLOT(outputActionTriggered(bool)));

    ui->outputTabWidget->setFocusPolicy(Qt::NoFocus);
    connect(ui->outputTabWidget, SIGNAL(currentChanged(int)), this, SLOT(outputTabSwitched(int)));
    if (dockWidgetArea(ui->outputDockWidget) == Qt::RightDockWidgetArea) {
        ui->outputTabWidget->setTabPosition(QTabWidget::East);
    } else if (dockWidgetArea(ui->outputDockWidget) == Qt::LeftDockWidgetArea) {
        ui->outputTabWidget->setTabPosition(QTabWidget::West);
    } else {
        ui->outputTabWidget->setTabPosition(QTabWidget::North);
    }
    QDockWidget::DockWidgetFeatures ofeatures = ui->outputDockWidget->features();
    if (dockWidgetArea(ui->outputDockWidget) == Qt::RightDockWidgetArea && (ofeatures & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->outputDockWidget->setFeatures(ofeatures ^ QDockWidget::DockWidgetVerticalTitleBar);
    } else if (dockWidgetArea(ui->outputDockWidget) != Qt::RightDockWidgetArea && !(ofeatures & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->outputDockWidget->setFeatures(ofeatures | QDockWidget::DockWidgetVerticalTitleBar);
    }
    connect(ui->outputDockWidget, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(outputDockLocationChanged(Qt::DockWidgetArea)));

    ui->sidebarTabWidget->setFocusPolicy(Qt::NoFocus);
    if (dockWidgetArea(ui->sidebarDockWidget) == Qt::RightDockWidgetArea) {
        ui->sidebarTabWidget->setTabPosition(QTabWidget::East);
    } else {
        ui->sidebarTabWidget->setTabPosition(QTabWidget::West);
    }
    QDockWidget::DockWidgetFeatures sfeatures = ui->sidebarDockWidget->features();
    if (dockWidgetArea(ui->sidebarDockWidget) == Qt::RightDockWidgetArea && (sfeatures & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->sidebarDockWidget->setFeatures(sfeatures ^ QDockWidget::DockWidgetVerticalTitleBar);
    } else if (dockWidgetArea(ui->sidebarDockWidget) != Qt::RightDockWidgetArea && !(sfeatures & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->sidebarDockWidget->setFeatures(sfeatures | QDockWidget::DockWidgetVerticalTitleBar);
    }
    connect(ui->sidebarDockWidget, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(sidebarDockLocationChanged(Qt::DockWidgetArea)));

    bool showDockButtons = false;
    if (Settings::get("show_dock_buttons") == "yes") showDockButtons = true;
    if (!showDockButtons) {
        QWidget * sidebarTitleBarWidget = new DockTitleBar();
        ui->sidebarDockWidget->setTitleBarWidget(sidebarTitleBarWidget);
        QWidget * outputTitleBarWidget = new DockTitleBar();
        ui->outputDockWidget->setTitleBarWidget(outputTitleBarWidget);
    }

    searchResultsColor = QColor(QString::fromStdString(Settings::get("search_results_color")));
    outputColor = QColor(QString::fromStdString(Settings::get("output_color")));
    outputBgColor = QColor(QString::fromStdString(Settings::get("output_bg_color")));

    // output tabs font
    QFont outputFont;
    std::string editorFontFamily = Settings::get("editor_font_family");
    std::string editorFontSize = Settings::get("editor_font_size");
    if (editorFontFamily=="") {
        QFont sysFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        outputFont.setFamily(sysFont.family());
    } else {
        outputFont.setStyleHint(QFont::Monospace);
        outputFont.setFamily(QString::fromStdString(editorFontFamily));
    }
    outputFont.setPointSize(std::stoi(editorFontSize));
    outputFont.setStyleName("");
    ui->messagesBrowser->setFont(outputFont);
    ui->helpBrowser->setFont(outputFont);
    ui->searchListWidget->setFont(outputFont);
    ui->outputEdit->setFont(outputFont);
    ui->todoEdit->setFont(outputFont);

    if (terminal != nullptr) {
        terminal->setFont(outputFont);

        QVBoxLayout * terminalLayout = new QVBoxLayout();
        terminalLayout->setContentsMargins(3, 3, 3, 3);
        terminalLayout->addWidget(terminal->getWidget());
        QWidget * terminalTab = new QWidget(ui->outputTabWidget);
        terminalTab->setLayout(terminalLayout);
        terminalTab->setStyleSheet("background:black");
        terminalTabIndex = ui->outputTabWidget->addTab(terminalTab, tr("Terminal"));

        QString shortcutCopyStr = QString::fromStdString("Ctrl+Shift+C");
        QShortcut * shortcutCopy = new QShortcut(QKeySequence(shortcutCopyStr), this);
        connect(shortcutCopy, SIGNAL(activated()), this, SLOT(terminalCopy()));

        QString shortcutPasteStr = QString::fromStdString("Ctrl+Shift+V");
        QShortcut * shortcutPaste = new QShortcut(QKeySequence(shortcutPasteStr), this);
        connect(shortcutPaste, SIGNAL(activated()), this, SLOT(terminalPaste()));

        QString showTerminalStr = QString::fromStdString(Settings::get("shortcut_terminal"));
        QShortcut * showTerminal = new QShortcut(QKeySequence(showTerminalStr), this);
        connect(showTerminal, SIGNAL(activated()), this, SLOT(showTerminal()));

        QTimer::singleShot(TERMINAL_START_DELAY, this, SLOT(startTerminal()));
    } else {
        terminalTabIndex = -1;
    }

    QPalette outputPalette;
    outputPalette.setColor(QPalette::Base, outputBgColor);
    outputPalette.setColor(QPalette::Text, outputColor);
    ui->messagesBrowser->setPalette(outputPalette);
    ui->helpBrowser->setPalette(outputPalette);
    ui->searchListWidget->setPalette(outputPalette);
    ui->outputEdit->setPalette(outputPalette);
    ui->todoEdit->setPalette(outputPalette);

    // settings
    if (!gitCommandsEnabled) {
        QList<QAction *> gitActions = ui->menuGit->actions();
        foreach (QAction * action, gitActions) {
            action->setEnabled(false);
        }
    }
    if (!serverCommandsEnabled) {
        QList<QAction *> toolsActions = ui->menuTools->actions();
        foreach (QAction * action, toolsActions) {
            if (action->objectName() == "actionServersStatus" ||
                action->objectName() == "actionStartServers" ||
                action->objectName() == "actionStopServers"
            ) {
                action->setEnabled(false);
            }
        }
    }
    if (!parsePHPEnabled) {
        QList<QAction *> createActions = ui->menuCreate->actions();
        foreach (QAction * action, createActions) {
            if (action->objectName() == "actionNewProject"
            ) {
                action->setEnabled(false);
            }
        }
        QList<QAction *> fileActions = ui->menuFile->actions();
        foreach (QAction * action, fileActions) {
            if (action->objectName() == "actionOpenProject" ||
                action->objectName() == "actionUpdateProject" ||
                action->objectName() == "actionRescanProject" ||
                action->objectName() == "actionEditProject"
            ) {
                action->setEnabled(false);
            }
        }
    }

    setWindowTitleText("");
    args = QCoreApplication::arguments();
    if (args.length() <= 1) {
        QTimer::singleShot(PROJECT_LOAD_DELAY, this, SLOT(projectLoadOnStart()));
    } else {
        QTimer::singleShot(PROJECT_LOAD_DELAY, this, SLOT(openFromArgs()));
    }

    // shortcuts
    QString shortcutSidebarStr = QString::fromStdString(Settings::get("shortcut_sidebar"));
    QShortcut * shortcutSidebar = new QShortcut(QKeySequence(shortcutSidebarStr), this);
    connect(shortcutSidebar, SIGNAL(activated()), this, SLOT(on_actionShowHideSidebar_triggered()));

    QString shortcutToobarStr = QString::fromStdString(Settings::get("shortcut_toolbar"));
    QShortcut * shortcutToobar = new QShortcut(QKeySequence(shortcutToobarStr), this);
    connect(shortcutToobar, SIGNAL(activated()), this, SLOT(on_actionShowHideToolbar_triggered()));

    QString shortcutOutputStr = QString::fromStdString(Settings::get("shortcut_output"));
    QShortcut * shortcutOutput = new QShortcut(QKeySequence(shortcutOutputStr), this);
    connect(shortcutOutput, SIGNAL(activated()), this, SLOT(on_actionShowHideOutput_triggered()));

    QString shortcutQuickAccessStr = QString::fromStdString(Settings::get("shortcut_quick_access"));
    QShortcut * shortcutQuickAccess = new QShortcut(QKeySequence(shortcutQuickAccessStr), this);
    connect(shortcutQuickAccess, SIGNAL(activated()), this, SLOT(on_actionQuickAccess_triggered()));

    QString shortcutQuickAccessAltStr = QString::fromStdString(Settings::get("shortcut_quick_access_alt"));
    if (shortcutQuickAccessStr != shortcutQuickAccessAltStr) {
        QShortcut * shortcutQuickAccessAlt = new QShortcut(QKeySequence(shortcutQuickAccessAltStr), this);
        connect(shortcutQuickAccessAlt, SIGNAL(activated()), this, SLOT(on_actionQuickAccess_triggered()));
    }

    QString shortcutFocusTreeStr = QString::fromStdString(Settings::get("shortcut_focus_tree"));
    QShortcut * shortcutFocusTree = new QShortcut(QKeySequence(shortcutFocusTreeStr), this);
    connect(shortcutFocusTree, SIGNAL(activated()), this, SLOT(focusTreeTriggered()));

    QString shortcutOpenFileStr = QString::fromStdString(Settings::get("shortcut_open_file"));
    QShortcut * shortcutOpenFile = new QShortcut(QKeySequence(shortcutOpenFileStr), this);
    connect(shortcutOpenFile, SIGNAL(activated()), this, SLOT(on_actionOpenFile_triggered()));

    QString shortcutOpenProjectStr = QString::fromStdString(Settings::get("shortcut_open_project"));
    QShortcut * shortcutOpenProject = new QShortcut(QKeySequence(shortcutOpenProjectStr), this);
    connect(shortcutOpenProject, SIGNAL(activated()), this, SLOT(on_actionOpenProject_triggered()));

    QString shortcutNewFileStr = QString::fromStdString(Settings::get("shortcut_new_file"));
    QShortcut * shortcutNewFile = new QShortcut(QKeySequence(shortcutNewFileStr), this);
    connect(shortcutNewFile, SIGNAL(activated()), this, SLOT(on_actionNewFile_triggered()));

    QString shortcutNewFolderStr = QString::fromStdString(Settings::get("shortcut_new_folder"));
    QShortcut * shortcutNewFolder = new QShortcut(QKeySequence(shortcutNewFolderStr), this);
    connect(shortcutNewFolder, SIGNAL(activated()), this, SLOT(on_actionNewFolder_triggered()));

    QString shortcutPreviousTabStr = QString::fromStdString(Settings::get("shortcut_previous_tab"));
    QShortcut * shortcutPreviousTab = new QShortcut(QKeySequence(shortcutPreviousTabStr), this);
    connect(shortcutPreviousTab, SIGNAL(activated()), this, SLOT(previousTabTriggered()));

    QString shortcutNextTabStr = QString::fromStdString(Settings::get("shortcut_next_tab"));
    QShortcut * shortcutNextTab = new QShortcut(QKeySequence(shortcutNextTabStr), this);
    connect(shortcutNextTab, SIGNAL(activated()), this, SLOT(nextTabTriggered()));

    QString shortcutTabsListStr = QString::fromStdString(Settings::get("shortcut_tabs_list"));
    QShortcut * shortcutTabsList = new QShortcut(QKeySequence(shortcutTabsListStr), this);
    connect(shortcutTabsList, SIGNAL(activated()), this, SLOT(tabsListTriggered()));

    QString shortcutSplitTabStr = QString::fromStdString(Settings::get("shortcut_split_tab"));
    QShortcut * shortcutSplitTab = new QShortcut(QKeySequence(shortcutSplitTabStr), this);
    connect(shortcutSplitTab, SIGNAL(activated()), this, SLOT(on_actionSplitTab_triggered()));

    QString shortcutCloseTabStr = QString::fromStdString(Settings::get("shortcut_close_tab"));
    QShortcut * shortcutCloseTab = new QShortcut(QKeySequence(shortcutCloseTabStr), this);
    connect(shortcutCloseTab, SIGNAL(activated()), this, SLOT(on_actionClose_triggered()));

    QString shortcutCloseProjectStr = QString::fromStdString(Settings::get("shortcut_close_project"));
    QShortcut * shortcutCloseProject = new QShortcut(QKeySequence(shortcutCloseProjectStr), this);
    connect(shortcutCloseProject, SIGNAL(activated()), this, SLOT(on_actionCloseProject_triggered()));

    QString shortcutSaveAllStr = QString::fromStdString(Settings::get("shortcut_save_all"));
    QShortcut * shortcutSaveAll = new QShortcut(QKeySequence(shortcutSaveAllStr), this);
    connect(shortcutSaveAll, SIGNAL(activated()), this, SLOT(on_actionSaveAll_triggered()));

    QString shortcutSearchInFilesStr = QString::fromStdString(Settings::get("shortcut_search_in_files"));
    QShortcut * shortcutSearchInFIles = new QShortcut(QKeySequence(shortcutSearchInFilesStr), this);
    connect(shortcutSearchInFIles, SIGNAL(activated()), this, SLOT(on_actionSearchInFiles_triggered()));

    QString shortcutCloseAppStr = QString::fromStdString(Settings::get("shortcut_close_app"));
    QShortcut * shortcutCloseApp = new QShortcut(QKeySequence(shortcutCloseAppStr), this);
    connect(shortcutCloseApp, SIGNAL(activated()), this, SLOT(on_actionQuit_triggered()));

    QString shortcutExecuteStr = QString::fromStdString(Settings::get("shortcut_execute"));
    QShortcut * shortcutExecute = new QShortcut(QKeySequence(shortcutExecuteStr), this);
    connect(shortcutExecute, SIGNAL(activated()), this, SLOT(on_actionExecuteFile_triggered()));

    QString shortcutExecuteSelectionStr = QString::fromStdString(Settings::get("shortcut_execute_selection"));
    QShortcut * shortcutExecuteSelection = new QShortcut(QKeySequence(shortcutExecuteSelectionStr), this);
    connect(shortcutExecuteSelection, SIGNAL(activated()), this, SLOT(on_actionExecuteSelection_triggered()));

    connect(QApplication::inputMethod(), SIGNAL(visibleChanged()), this, SLOT(inputMethodVisibleChanged()));

    if (Settings::get("scale_factor_unchecked") == "yes" && Settings::get("scale_auto") == "no" &&  Settings::get("enable_scaling") == "yes") {
        QTimer::singleShot(CHECK_SCALE_FACTOR_DELAY, this, SLOT(checkScaleFactor()));
    }

    suspended = false;
    // make sure that window is maximized in Android
    #if defined(Q_OS_ANDROID)
    setWindowState( windowState() | Qt::WindowMaximized);
    if (Settings::get("enable_android_desktop_mode") != "yes") {
        ui->menuBar->setVisible(false);
        ui->mainToolBar->setVisible(true);
        ui->mainToolBar->insertSeparator(ui->mainToolBar->actions().at(0));
        QAction * mainMenuAction = new QAction(tr("Menu"));
        mainMenuAction->setIcon(Icon::get("actionMenu", QIcon(":/icons/separator-double.png")));
        connect(mainMenuAction, SIGNAL(triggered(bool)), this, SLOT(mainMenuDialogTriggered(bool)));
        ui->mainToolBar->insertAction(ui->mainToolBar->actions().at(0), mainMenuAction);
    }
    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(applicationStateChanged(Qt::ApplicationState)));
    #endif

    bool autoShowVirtualKeyboard = false;
    if (Settings::get("auto_show_virtual_keyboard") == "yes") autoShowVirtualKeyboard = true;
    qApp->setAutoSipEnabled(autoShowVirtualKeyboard);

    MainWindow::WANT_RESTART = false;
}

MainWindow::~MainWindow()
{
    parserThread.quit();
    parserThread.wait();
    delete filebrowser;
    delete navigator;
    delete editorTabs;
    delete editorTabsSplit;
    delete gitBrowser;
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    #if defined(Q_OS_ANDROID)
    if (!MainWindow::WANT_RESTART && ui->outputDockWidget->isVisible()) {
        outputActionTriggered(false);
        event->ignore();
        return;
    }
    if (!MainWindow::WANT_RESTART && ui->sidebarDockWidget->isVisible()) {
        sidebarActionTriggered(false);
        event->ignore();
        return;
    }
    if (!MainWindow::WANT_RESTART && !Helper::showQuestion(tr("Confirmation"), tr("Do you want to exit ?"))) {
        MainWindow::WANT_RESTART = false;
        event->ignore();
        return;
    }
    MainWindow::WANT_RESTART = false;
    #endif
    // check modified
    if (!editorTabs->closeWindowAllowed() || !editorTabsSplit->closeWindowAllowed()) {
        event->ignore();
        return;
    }
    emit disableWorker();
    saveStateBeforeExit();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveStateBeforeExit()
{
    // save project
    project->save(editorTabs->getOpenTabFiles(), editorTabs->getOpenTabLines(), editorTabs->getCurrentTabIndex(), ui->todoEdit->toPlainText());
    if (Settings::get("devpack_install_silent") == "no") {
        std::unordered_map<std::string,std::string> sMap;
        sMap["devpack_install_silent"] = "yes";
        Settings::change(sMap);
    }
    Settings::save();
    // save wnd geometry & state
    QSettings windowSettings;
    windowSettings.setValue("main_window_geometry", saveGeometry());
    windowSettings.setValue("main_window_state", saveState());
    if (args.length() <= 1) {
        windowSettings.setValue("project_path", project->getPath());
    }
}

void MainWindow::menuEditOnShow()
{
    editorActionsChanged();
}

void MainWindow::checkScaleFactor()
{
    bool ok = true;
    std::unordered_map<std::string, std::string> settingsChanged;
    if (!Helper::showQuestion(tr("Scale factor"), tr("Do you want to keep specified scale factor ?"))) {
        settingsChanged["scale_auto"] = "yes";
        settingsChanged["scale_factor"] = "100";
        ok = false;
    }
    settingsChanged["scale_factor_unchecked"] = "no";
    Settings::change(settingsChanged);
    if (!ok) {
        MainWindow::WANT_RESTART = true;
        if (close()) {
            QApplication::exit();
        } else {
            MainWindow::WANT_RESTART = false;
        }
    }
}

void MainWindow::menuViewOnShow()
{
    QList<QAction *> viewActions = ui->menuView->actions();
    foreach (QAction * action, viewActions) {
        if (action->objectName() == "actionShowHideSidebar") {
            if (ui->sidebarDockWidget->isVisible()) action->setChecked(true);
            else action->setChecked(false);
        } else if (action->objectName() == "actionShowHideToolbar") {
            if (ui->mainToolBar->isVisible()) action->setChecked(true);
            else action->setChecked(false);
        } else if (action->objectName() == "actionShowHideOutput") {
            if (ui->outputDockWidget->isVisible()) action->setChecked(true);
            else action->setChecked(false);
        } else if (action->objectName() == "actionDisplayDockButtons") {
            bool showDockButtons = false;
            if (Settings::get("show_dock_buttons") == "yes") showDockButtons = true;
            action->setChecked(showDockButtons);
        }
    }
}

void MainWindow::menuToolsOnShow()
{
    bool sassEnabled = false;
    bool execEnabled = false;
    bool execSelectionEnabled = false;
    Editor * textEditor = getActiveEditor();
    if (textEditor != nullptr) {
        QString ext = textEditor->getFileExtension().toLower();
        if (!textEditor->isModified() && (ext == "scss" || ext == "sass")) {
            sassEnabled = true;
        } else if (ext == "php") {
            if (!textEditor->isModified()) execEnabled = true;
            if (textEditor->textCursor().selectedText().size() > 0) {
                execSelectionEnabled = true;
            }
        }
    }
    QList<QAction *> toolsActions = ui->menuTools->actions();
    foreach (QAction * action, toolsActions) {
        if (action->objectName() == "actionCompileSass") {
            action->setEnabled(sassEnabled);
        } else if (action->objectName() == "actionExecuteFile") {
            action->setEnabled(execEnabled);
        } else if (action->objectName() == "actionExecuteSelection") {
            action->setEnabled(execSelectionEnabled);
        }
    }
}

Editor * MainWindow::getActiveEditor()
{
    Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
    if (textEditorSplit != nullptr && isSplitActive) {
        return textEditorSplit;
    }
    return editorTabs->getActiveEditor();
}

QString MainWindow::getCurrentTabFilename()
{
    Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
    if (textEditorSplit != nullptr && isSplitActive) {
        return editorTabsSplit->getCurrentTabFilename();
    }
    return editorTabs->getCurrentTabFilename();
}

void MainWindow::editorUndoRedoChanged()
{
    editorActionsChanged();
}

void MainWindow::editorBackForwardChanged()
{
    editorActionsChanged();
}

void MainWindow::editorActionsChanged()
{
    bool undo = false, redo = false, back = false, forward = false, rename = false;
    Editor * textEditor = getActiveEditor();
    if (textEditor != nullptr) {
        undo = textEditor->isUndoable();
        redo = textEditor->isRedoable();
        back = textEditor->isBackable();
        forward = textEditor->isForwadable();
        rename = true;
    }
    bool multiSelect = true;
    #if defined(Q_OS_ANDROID)
    multiSelect = false;
    #endif
    QList<QAction *> editActions = ui->menuEdit->actions();
    foreach (QAction * action, editActions) {
        if (action->objectName() == "actionUndo") {
            action->setEnabled(undo);
        } else if (action->objectName() == "actionRedo") {
            action->setEnabled(redo);
        } else if (action->objectName() == "actionBack") {
            action->setEnabled(back);
        } else if (action->objectName() == "actionForward") {
            action->setEnabled(forward);
        } else if (action->objectName() == "actionRename") {
            action->setEnabled(rename);
        } else if (action->objectName() == "actionMultiSelect") {
            action->setEnabled(multiSelect);
        }
    }
}

void MainWindow::disableActionsForEmptyTabs()
{
    QList<QAction *> fileActions = ui->menuFile->actions();
    foreach (QAction * action, fileActions) {
        if (action->objectName() == "actionSave" ||
            action->objectName() == "actionSaveAll" ||
            action->objectName() == "actionSaveAs" ||
            action->objectName() == "actionClose"
        ) {
            action->setEnabled(false);
        }
    }
    QList<QAction *> editActions = ui->menuEdit->actions();
    foreach (QAction * action, editActions) {
        if (action->objectName() == "actionUndo" ||
            action->objectName() == "actionRedo" ||
            action->objectName() == "actionBack" ||
            action->objectName() == "actionForward" ||
            action->objectName() == "actionFindReplace"
        ) {
            action->setEnabled(false);
        }
    }
    QList<QAction *> toolsActions = ui->menuTools->actions();
    foreach (QAction * action, toolsActions) {
        if (action->objectName() == "actionSplitTab"
        ) {
            action->setEnabled(false);
        }
    }
    if (gitCommandsEnabled) {
        QList<QAction *> gitActions = ui->menuGit->actions();
        foreach (QAction * action, gitActions) {
            if (action->objectName() == "actionGitDiffCurrent" ||
                action->objectName() == "actionGitAddCurrent" ||
                action->objectName() == "actionGitResetCurrent" ||
                action->objectName() == "actionGitDiffCurrentCommit"
            ) {
                action->setEnabled(false);
            }
        }
    }
}

void MainWindow::enableActionsForOpenTabs()
{
    QList<QAction *> fileActions = ui->menuFile->actions();
    foreach (QAction * action, fileActions) {
        if (action->objectName() == "actionSave" ||
            action->objectName() == "actionSaveAll" ||
            action->objectName() == "actionSaveAs" ||
            action->objectName() == "actionClose"
        ) {
            action->setEnabled(true);
        }
    }
    QList<QAction *> editActions = ui->menuEdit->actions();
    foreach (QAction * action, editActions) {
        if (action->objectName() == "actionFindReplace") {
            action->setEnabled(true);
        }
    }
    QList<QAction *> toolsActions = ui->menuTools->actions();
    foreach (QAction * action, toolsActions) {
        if (action->objectName() == "actionSplitTab"
        ) {
            action->setEnabled(true);
        }
    }
    if (gitCommandsEnabled) {
        QList<QAction *> gitActions = ui->menuGit->actions();
        foreach (QAction * action, gitActions) {
            if (action->objectName() == "actionGitDiffCurrent" ||
                action->objectName() == "actionGitAddCurrent" ||
                action->objectName() == "actionGitResetCurrent" ||
                action->objectName() == "actionGitDiffCurrentCommit"
            ) {
                action->setEnabled(true);
            }
        }
    }
}

void MainWindow::disableActionsForEmptyProject()
{
    QList<QAction *> fileActions = ui->menuFile->actions();
    foreach (QAction * action, fileActions) {
        if (action->objectName() == "actionCloseProject" ||
            action->objectName() == "actionUpdateProject" ||
            action->objectName() == "actionRescanProject" ||
            action->objectName() == "actionEditProject"
        ) {
            action->setEnabled(false);
        }
    }
    ui->todoTab->setEnabled(false);
}

void MainWindow::enableActionsForOpenProject()
{
    QList<QAction *> fileActions = ui->menuFile->actions();
    foreach (QAction * action, fileActions) {
        if (action->objectName() == "actionCloseProject" ||
            action->objectName() == "actionUpdateProject" ||
            action->objectName() == "actionRescanProject" ||
            action->objectName() == "actionEditProject"
        ) {
            action->setEnabled(true);
        }
    }
    ui->todoTab->setEnabled(true);
}

void MainWindow::projectLoadOnStart()
{
    showWelcomeScreen();
    /*
    #if defined(Q_OS_ANDROID)
    emit installAndroidPack();
    #endif
    */
    QSettings windowSettings;
    QString projectPath = windowSettings.value("project_path").toString();
    if (projectPath.size() > 0 && Helper::folderExists(projectPath) && project->exists(projectPath)) {
        projectOpenRequested(projectPath);
    }
}

void MainWindow::openFromArgs() {
    #if defined(Q_OS_ANDROID)
    emit installAndroidPack();
    #endif
    if (args.length() <= 1) return;
    QStringList files;
    for (int i=1; i<args.length(); i++) {
        QString arg = args[i];
        if (!Helper::fileExists(arg)) continue;
        files.append(arg);
    }
    openTabsRequested(files, false);
    editorTabs->initHighlighters();
}

void MainWindow::previousTabTriggered()
{
    if (ui->tabWidget->count() < 2) {
        Editor * textEditor = editorTabs->getActiveEditor();
        if (textEditor != nullptr) textEditor->setFocus();
        return;
    }
    int index = ui->tabWidget->currentIndex();
    index--;
    if (index < 0) index = ui->tabWidget->count()-1;
    editorTabs->setActiveTab(index);
}

void MainWindow::nextTabTriggered()
{
    if (ui->tabWidget->count() < 2) {
        Editor * textEditor = editorTabs->getActiveEditor();
        if (textEditor != nullptr) textEditor->setFocus();
        return;
    }
    int index = ui->tabWidget->currentIndex();
    index++;
    if (index >= ui->tabWidget->count()) index = 0;
    editorTabs->setActiveTab(index);
}

void MainWindow::focusTreeTriggered()
{
    if (!ui->sidebarDockWidget->isVisible()) ui->sidebarDockWidget->show();
    if (!filebrowser->isFocused() && !navigator->isFocused()) {
        ui->sidebarTabWidget->setCurrentIndex(SIDEBAR_TAB_FILE_BROWSER_INDEX);
        filebrowser->focus();
    } else if (!navigator->isFocused() && !gitBrowser->isFocused()) {
        ui->sidebarTabWidget->setCurrentIndex(SIDEBAR_TAB_NAVIGATOR_INDEX);
        navigator->focus();
    } else if (!gitBrowser->isFocused() && !filebrowser->isFocused()) {
        ui->sidebarTabWidget->setCurrentIndex(SIDEBAR_TAB_GIT_BROWSER_INDEX);
        gitBrowser->focus();
    }
}

void MainWindow::fileBrowserOpen(QString file)
{
    editorTabs->openFile(file);
    if (filesHistory.contains(file)) {
        editorShowLine(filesHistory[file]);
    }
}

void MainWindow::mainMenuDialogTriggered(bool)
{
    MenuDialog menuDialog(ui->menuBar, this);
    connect(&menuDialog, SIGNAL(showContextMenu()), this, SLOT(on_actionOpenContextMenu_triggered()));
    connect(&menuDialog, SIGNAL(showPreferences()), this, SLOT(on_actionSettings_triggered()));
    connect(&menuDialog, SIGNAL(quit()), this, SLOT(on_actionQuit_triggered()));
    menuDialog.build();
    menuDialog.exec();
}

void MainWindow::on_actionSplitTab_triggered()
{
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor != nullptr) {
        QString fileName = textEditor->getFileName();
        if (fileName.size() > 0 && Helper::fileExists(fileName)) {
            Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
            if (textEditorSplit != nullptr && textEditorSplit->getFileName() == fileName) {
                editorTabsSplit->closeTab(textEditorSplit->getTabIndex());
            } else {
                if (!tabWidgetSplit->isVisible()) {
                    tabWidgetSplit->show();
                    int w = ui->centralWidget->geometry().width() / 2;
                    editorsSplitter->setSizes(QList<int>() << w << w);
                }
                editorTabsSplit->openFile(fileName);
                textEditorSplit = editorTabsSplit->getActiveEditor();
                if (textEditorSplit != nullptr && textEditorSplit->getFileName() == fileName) {
                    textEditorSplit->gotoLine(textEditor->getCursorLine());
                }
            }
        }
    }
}

void MainWindow::on_actionOpenContextMenu_triggered()
{
    QTimer::singleShot(100, this, SLOT(sendContextMenuEvent()));
}

void MainWindow::sendContextMenuEvent()
{
    QWidget * widget = QApplication::focusWidget();
    if (widget == nullptr) return;
    QContextMenuEvent * contextEvent = new QContextMenuEvent(QContextMenuEvent::Keyboard, widget->mapFromGlobal(QCursor::pos()));
    QCoreApplication::postEvent(widget, contextEvent);
}

void MainWindow::on_actionOpenFile_triggered()
{
    setStatusBarText("");
    editorTabs->open(filebrowser->getRootPath());
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor != nullptr && filesHistory.contains(textEditor->getFileName())) {
        editorShowLine(filesHistory[textEditor->getFileName()]);
    }
}

void MainWindow::on_actionNewFile_triggered()
{
    filebrowser->showCreateFileDialog();
}

void MainWindow::on_actionNewFolder_triggered()
{
    filebrowser->showCreateFolderDialog();
}

void MainWindow::on_actionRename_triggered()
{
    QString fileName = getCurrentTabFilename();
    if (fileName.size() == 0 || !Helper::fileExists(fileName)) return;
    filebrowser->showRenameDialog(fileName);
}

void MainWindow::on_actionNewProject_triggered()
{
    filebrowser->showCreateProjectDialog(parsePHPLintEnabled, parsePHPCSEnabled);
}

void MainWindow::on_actionEditProject_triggered()
{
    if (!project->isOpen()) return;
    filebrowser->showEditProjectDialog(project->getName(), project->getPath(), project->isPHPLintEnabled(), project->isPHPCSEnabled());
}

void MainWindow::on_actionOpenProject_triggered()
{
    filebrowser->openProject();
}

void MainWindow::on_actionUpdateProject_triggered()
{
    if (!project->isOpen()) return;
    setStatusBarText(tr("Scanning project..."));
    emit parseProject(project->getPath());
}

void MainWindow::on_actionRescanProject_triggered()
{
    if (!project->isOpen()) return;
    project->deleteDataFile();
    setStatusBarText(tr("Scanning project..."));
    emit parseProject(project->getPath());
}

void MainWindow::on_actionCloseProject_triggered()
{
    project->save(editorTabs->getOpenTabFiles(), editorTabs->getOpenTabLines(), editorTabs->getCurrentTabIndex(), ui->todoEdit->toPlainText());
    project->close();
    reloadWords();
    disableActionsForEmptyProject();
    filebrowser->rebuildFileBrowserTree(filebrowser->getHomeDir());
    resetLastSearchParams();
    // update window title
    setWindowTitleText("");
    gitTabRefreshRequested();
    if (terminal != nullptr) {
        terminal->changeDir(QDir::homePath());
    }
}

void MainWindow::on_actionSave_triggered()
{
    Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
    if (textEditorSplit != nullptr && isSplitActive) {
        editorTabsSplit->save();
    } else {
        editorTabs->save();
    }
}

void MainWindow::on_actionSaveAll_triggered()
{
    editorTabs->saveAll();
    editorTabsSplit->saveAll();
}

void MainWindow::on_actionSaveAs_triggered()
{
    Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
    if (textEditorSplit != nullptr && isSplitActive) {
        editorTabsSplit->saveAs();
    } else {
        editorTabs->saveAs();
    }
}

void MainWindow::on_actionClose_triggered()
{
    Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
    if (textEditorSplit != nullptr && isSplitActive) {
        editorTabsSplit->close();
    } else {
        editorTabs->close();
    }
}

void MainWindow::on_actionQuit_triggered()
{
    #if defined(Q_OS_ANDROID)
    MainWindow::WANT_RESTART = true; // force quit (restart is disabled on Android)
    #endif
    close();
}

void MainWindow::on_actionUndo_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->undo();
}

void MainWindow::on_actionRedo_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->redo();
}

void MainWindow::on_actionBack_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->back();
}

void MainWindow::on_actionForward_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->forward();
}

void MainWindow::on_actionFindReplace_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->findToggle();
}

void MainWindow::on_actionColorPicker_triggered()
{
    Editor * textEditor = getActiveEditor();
    QColor initColor = Qt::white;
    bool withHash = true;
    if (textEditor != nullptr) {
        QTextCursor curs = textEditor->textCursor();
        QString text = curs.selectedText();
        if (text.size() > 0) {
            if (text.at(0) != "#") {
                withHash = false;
                text = "#" + text;
            }
            QColor textColor = QColor(text);
            if (textColor.isValid()) initColor = textColor;
        }
    }
    //QColor color = QColorDialog::getColor(initColor, this, tr("Pick a color"));
    ColorDialog dialog(this);
    dialog.setCurrentColor(initColor);
    if (!dialog.exec()) return;
    QColor color = dialog.selectedColor();
    if (!color.isValid()) return;
    if (textEditor != nullptr) {
        QTextCursor curs = textEditor->textCursor();
        QString text = color.name();
        if (!withHash && text.size() > 0 && text.at(0) == "#") text = text.mid(1);
        curs.insertText(text);
        textEditor->setTextCursor(curs);
        textEditor->setFocus();
    }
}

void MainWindow::on_actionSearchInFiles_triggered()
{
    QString text = "";
    Editor * textEditor = getActiveEditor();
    if (textEditor != nullptr) {
        QTextCursor curs = textEditor->textCursor();
        text = curs.selectedText();
    }
    editorSearchInFilesRequested(text);
}

void MainWindow::on_actionMultiSelect_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->multiSelectToggle();
}

void MainWindow::on_actionShowHideSidebar_triggered()
{
    hideQAPanel();
    if (ui->sidebarDockWidget->isVisible()) {
        ui->sidebarDockWidget->hide();
    } else {
        ui->sidebarDockWidget->show();
        ui->sidebarTabWidget->setFocus();
    }
}

void MainWindow::on_actionShowHideToolbar_triggered()
{
    hideQAPanel();
    if (ui->mainToolBar->isVisible()) {
        ui->mainToolBar->hide();
    } else {
        ui->mainToolBar->show();
    }
}

void MainWindow::on_actionShowHideOutput_triggered()
{
    hideQAPanel();
    if (ui->outputDockWidget->isVisible()) {
        ui->outputDockWidget->hide();
    } else {
        ui->outputDockWidget->show();
        if (terminal != nullptr && ui->outputTabWidget->currentIndex() == terminalTabIndex) {
            terminal->getWidget()->setFocus();
        } else {
            ui->outputTabWidget->setFocus();
        }
    }
}

void MainWindow::outputTabSwitched(int index)
{
    if (terminal != nullptr && index == terminalTabIndex) {
        terminal->getWidget()->setFocus();
    }
}

void MainWindow::on_actionDisplayDockButtons_triggered()
{
    QWidget * oldSidebarTitleBarWidget = ui->sidebarDockWidget->titleBarWidget();
    QWidget * oldOutputTitleBarWidget = ui->outputDockWidget->titleBarWidget();

    bool showDockButtons = false;
    if (Settings::get("show_dock_buttons") == "yes") showDockButtons = true;
    std::unordered_map<std::string, std::string> data;
    if (showDockButtons) {
        QWidget * sidebarTitleBarWidget = new DockTitleBar();
        ui->sidebarDockWidget->setTitleBarWidget(sidebarTitleBarWidget);
        QWidget * outputTitleBarWidget = new DockTitleBar();
        ui->outputDockWidget->setTitleBarWidget(outputTitleBarWidget);
        data["show_dock_buttons"] = "no";
    } else {
        ui->sidebarDockWidget->setTitleBarWidget(nullptr);
        ui->outputDockWidget->setTitleBarWidget(nullptr);
        data["show_dock_buttons"] = "yes";
    }

    Settings::change(data);
    if (oldSidebarTitleBarWidget != nullptr) oldSidebarTitleBarWidget->deleteLater();
    if (oldOutputTitleBarWidget != nullptr) oldOutputTitleBarWidget->deleteLater();
}

void MainWindow::on_actionQuickAccess_triggered()
{
    if (!qa->isVisible()) showQAPanel();
    else hideQAPanel();
}

void MainWindow::on_actionStartServers_triggered()
{
    QString pwd = Helper::showInputDialog(tr("Enter root password"), tr("Password:"), QLineEdit::Password);
    if (pwd.isNull()) return;
    runServersCommand(SERVERS_START_CMD, pwd, tr("Starting apache2 and mariadb servers..."));
}

void MainWindow::on_actionStopServers_triggered()
{
    QString pwd = Helper::showInputDialog(tr("Enter root password"), tr("Password:"), QLineEdit::Password);
    if (pwd.isNull()) return;
    runServersCommand(SERVERS_STOP_CMD, pwd, tr("Stopping apache2 and mariadb servers..."));
}

void MainWindow::on_actionServersStatus_triggered()
{
    QString pwd = Helper::showInputDialog(tr("Enter root password"), tr("Password:"), QLineEdit::Password);
    if (pwd.isNull()) return;
    runServersCommand(SERVERS_STATUS_CMD, pwd, tr("Fetching status of apache2 and mariadb servers..."));
}

void MainWindow::runServersCommand(QString command, QString pwd, QString description)
{
    if (!serverCommandsEnabled) return;
    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_RESULTS_INDEX);
    ui->outputEdit->clear();
    ui->outputEdit->setHtml(Servers::highlightServersCommand(description));
    emit serversCommand(command, pwd);
}

void MainWindow::serversCommandFinished(QString output)
{
    output = output.trimmed() + "\n\n" + tr("Finished.");
    ui->outputEdit->append(Servers::highlightServersCommandOutput(output));
    QTextCursor cursor = ui->outputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    ui->outputEdit->setTextCursor(cursor);
}

void MainWindow::on_actionCompileSass_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr || textEditor->isModified()) return;
    QString ext = textEditor->getFileExtension().toLower();
    if (ext != "scss" && ext != "sass") return;
    QString fileName = textEditor->getFileName();
    QFileInfo fInfo(fileName);
    QString file = fInfo.baseName() + ".css";
    QString path = fInfo.absolutePath();
    file = Helper::showInputDialog(tr("Enter filename"), tr("Filename:"), QLineEdit::Normal, file);
    if (file.isNull()) return;
    compileSass(fileName, path + "/" + file);
}

void MainWindow::compileSass(QString src, QString dst)
{
    if (!Helper::fileExists(src) || dst.size() == 0) return;
    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_RESULTS_INDEX);
    ui->outputEdit->clear();
    ui->outputEdit->setText(src + " >> " + dst + "\n");
    emit sassCommand(src, dst);
}

void MainWindow::sassCommandFinished(QString output, QString directory)
{
    output = output.trimmed();
    if (output.size() == 0) {
        output = tr("Finished.");
        filebrowser->refreshFileBrowserDirectory(directory);
    }
    ui->outputEdit->append(output);
    QTextCursor cursor = ui->outputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    ui->outputEdit->setTextCursor(cursor);
}

void MainWindow::on_actionExecuteFile_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr || textEditor->isModified()) return;
    QString ext = textEditor->getFileExtension().toLower();
    if (ext != "php") return;
    QString fileName = textEditor->getFileName();

    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_RESULTS_INDEX);
    ui->outputEdit->clear();
    QString cmdStr = "php -d max_execution_time=30 -f "+fileName;
    ui->outputEdit->setHtml(Servers::highlightServersCommand(cmdStr));

    emit execPHP(textEditor->getTabIndex(), fileName);
}

void MainWindow::on_actionExecuteSelection_triggered()
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    QString ext = textEditor->getFileExtension().toLower();
    if (ext != "php") return;
    QTextCursor cursor = textEditor->textCursor();
    QString text = cursor.selectedText();
    if (text.size() == 0) return;
    QString code = QString(text);
    text.replace("'","'\"'\"'").replace("<","&lt;").replace(">","&gt;").replace("\t","    ").replace(" ","&nbsp;");
    code.replace(QString::fromWCharArray(L"\u2029"),"\n");

    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_RESULTS_INDEX);
    ui->outputEdit->clear();
    QString cmdStr = "php -d max_execution_time=30 -r '"+text+"'";
    ui->outputEdit->setHtml(Servers::highlightServersCommand(cmdStr));

    emit execSelection(textEditor->getTabIndex(), code);
}

void MainWindow::on_actionStartPHPWebServer_triggered()
{
    emit startPHPWebServer(filebrowser->getRootPath());
}

void MainWindow::on_actionStopPHPWebServer_triggered()
{
    emit stopPHPWebServer();
}

void MainWindow::execPHPWebServerFinished(bool success, QString output)
{
    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_RESULTS_INDEX);
    ui->outputEdit->clear();
    ui->outputEdit->setText(output);
    if (success) {
        QDesktopServices::openUrl(QUrl("http://" + PHP_WEBSERVER_URI));
    }
}

void MainWindow::execPHPFinished(int tabIndex, QString output)
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    if (tabIndex != textEditor->getTabIndex()) return;

    int maxSize = 1046576;
    if (output.size() == 0) output = tr("Finished.");
    else if (output.size() > maxSize) output = output.mid(0, maxSize) + "\n" + tr("Too many results. Breaking...");
    ui->outputEdit->append(Servers::highlightServersCommandOutput(output));
    QTextCursor cursor = ui->outputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    ui->outputEdit->setTextCursor(cursor);
    ui->outputEdit->setFocus();
}

QString MainWindow::getGitWorkingDir()
{
    QString path;
    if (project->isOpen()) {
        path = project->getPath();
    } else {
        path = filebrowser->getRootPath();
    }
    return path;
}

void MainWindow::on_actionGitStatus_triggered()
{
    git->showStatus(getGitWorkingDir());
}

void MainWindow::on_actionGitLog_triggered()
{
    git->showLog(getGitWorkingDir());
}

void MainWindow::on_actionGitDiffTree_triggered()
{
    git->showLastCommitDiffTree(getGitWorkingDir());
}

void MainWindow::on_actionGitDiffAll_triggered()
{
    git->showUncommittedDiffAll(getGitWorkingDir());
}

void MainWindow::on_actionGitDiffCurrent_triggered()
{
    QString fileName = getCurrentTabFilename();
    if (fileName.size() == 0 || !Helper::fileExists(fileName)) return;
    git->showUncommittedDiffCurrent(getGitWorkingDir(), fileName);
}

void MainWindow::on_actionGitDiffAllCommit_triggered()
{
    git->showLastCommitDiffAll(getGitWorkingDir());
}

void MainWindow::on_actionGitDiffCurrentCommit_triggered()
{
    QString fileName = getCurrentTabFilename();
    if (fileName.size() == 0 || !Helper::fileExists(fileName)) return;
    git->showLastCommitDiffCurrent(getGitWorkingDir(), fileName);
}

void MainWindow::on_actionGitDiscardChanges_triggered()
{
    git->resetHardUncommitted(getGitWorkingDir());
}

void MainWindow::on_actionGitCancelCommit_triggered()
{
    git->resetToPreviousCommit(getGitWorkingDir());
}

void MainWindow::on_actionGitDiscardCommit_triggered()
{
    git->resetHardToPreviousCommit(getGitWorkingDir());
}

void MainWindow::on_actionGitRevert_triggered()
{
    git->revertLastCommit(getGitWorkingDir());
}

void MainWindow::on_actionGitResetAll_triggered()
{
    git->resetAll(getGitWorkingDir());
}

void MainWindow::on_actionGitResetCurrent_triggered()
{
    QString fileName = getCurrentTabFilename();
    if (fileName.size() == 0 || !Helper::fileExists(fileName)) return;
    git->resetCurrent(getGitWorkingDir(), fileName);
}

void MainWindow::on_actionGitAddAll_triggered()
{
    git->addAll(getGitWorkingDir());
}

void MainWindow::on_actionGitAddCurrent_triggered()
{
    QString fileName = getCurrentTabFilename();
    if (fileName.size() == 0 || !Helper::fileExists(fileName)) return;
    git->addCurrent(getGitWorkingDir(), fileName);
}

void MainWindow::on_actionGitCommit_triggered(bool add)
{
    QString msg = Helper::showInputDialog(tr("Commit message"), tr("Message:"), QLineEdit::Normal);
    if (msg.isNull()) return;
    if (!add) git->commit(getGitWorkingDir(), msg);
    else git->addAndCommit(getGitWorkingDir(), msg);
}

void MainWindow::on_actionGitPush_triggered()
{
    git->pushOriginMaster(getGitWorkingDir());
}

void MainWindow::on_actionGitPull_triggered()
{
    git->pullOriginMaster(getGitWorkingDir());
}

void MainWindow::on_actionGitInitializeRepository_triggered()
{
    git->initialize(getGitWorkingDir());
}

void MainWindow::on_actionGitAddRemoteURL_triggered()
{
    QString description = "";
    #if defined(Q_OS_ANDROID)
    description = "Note: you might want to add a username and password to repository URL\n(https://username:password@host/path)";
    #endif
    QString url = Helper::showInputDialog(tr("Add remote URL"), tr("Enter URL:"), QLineEdit::Normal, "", description);
    if (url.isNull() || url.size() == 0) return;
    git->addRemoteURL(getGitWorkingDir(), url);
}

void MainWindow::on_actionGitChangeRemoteURL_triggered()
{
    QString description = "";
    #if defined(Q_OS_ANDROID)
    description = "Note: you might want to add a username and password to repository URL\n(https://username:password@host/path)";
    #endif
    QString url = Helper::showInputDialog(tr("Change remote URL"), tr("Enter URL:"), QLineEdit::Normal, "", description);
    if (url.isNull() || url.size() == 0) return;
    git->changeRemoteURL(getGitWorkingDir(), url);
}

void MainWindow::on_actionGitCloneRepository_triggered()
{
    QString description = "";
    #if defined(Q_OS_ANDROID)
    description = "Note: you might want to add a username and password to repository URL\n(https://username:password@host/path)";
    #endif
    QString url = Helper::showInputDialog(tr("Clone repository"), tr("Enter URL:"), QLineEdit::Normal, "", description);
    if (url.isNull() || url.size() == 0) return;
    git->clone(getGitWorkingDir(), url);
}

void MainWindow::runGitCommand(QString path, QString command, QStringList attrs, bool outputResult, bool silent)
{
    if (!gitCommandsEnabled) return;
    if (!git->isCommandSafe(command) &&
        !Helper::showQuestion(tr("Are you sure ?"), tr("Do you really want to \"%1\" ?").arg(QString("git "+command+" "+attrs.join(" ")).trimmed()))
        //QMessageBox::question(this, tr("Are you sure ?"), tr("Do you really want to \"%1\" ?").arg(QString("git "+command+" "+attrs.join(" ")).trimmed()), QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok
    ) {
        return;
    }
    hideQAPanel();
    if (outputResult) {
        if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
        ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_RESULTS_INDEX);
        ui->outputEdit->clear();
        QString attrStr = "";
        for (int i=0; i<attrs.size(); i++) {
            QString attr = attrs.at(i);
            if (attrStr.size() > 0) attrStr += " ";
            if (attr.indexOf(" ") >= 0) attrStr += "'" + attr + "'";
            else attrStr += attr;
        }
        QString cmdStr = path+"> git "+command+" "+attrStr;
        ui->outputEdit->setHtml(git->highlightCommand(cmdStr));
    }
    emit gitCommand(path, command, attrs, outputResult, silent);
}

void MainWindow::gitCommandFinished(QString command, QString output, bool outputResult)
{
    if (!outputResult) {
        if (command == GIT_STATUS_COMMAND) {
            gitBrowser->build(output);
        } else if (command == GIT_ANNOTATION_COMMAND) {
            QHash<int, Git::Annotation> annotations = git->parseAnnotationOutput(output);
            Editor * textEditor = editorTabs->getActiveEditor();
            if (textEditor != nullptr && annotations.size() > 0 && annotations.contains(1) && textEditor->getFileName() == getGitWorkingDir() + "/" + annotations.value(1).file) {
                textEditor->setGitAnnotations(annotations);
            }
            Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
            if (textEditorSplit != nullptr && annotations.size() > 0 && annotations.contains(1) && textEditorSplit->getFileName() == getGitWorkingDir() + "/" + annotations.value(1).file) {
                textEditorSplit->setGitAnnotations(annotations);
            }
        } else if (command == GIT_DIFF_COMMAND) {
            QString file = "";
            QHash<int,Git::DiffLine> mLines = git->parseDiffUnifiedOutput(output, file);
            Editor * textEditor = editorTabs->getActiveEditor();
            if (textEditor != nullptr && (textEditor->getFileName() == getGitWorkingDir() + "/" + file || mLines.size() == 0)) {
                textEditor->setGitDiffLines(mLines);
            }
            Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
            if (textEditorSplit != nullptr && (textEditorSplit->getFileName() == getGitWorkingDir() + "/" + file || mLines.size() == 0)) {
                textEditorSplit->setGitDiffLines(mLines);
            }
        }
        return;
    }
    if (output.size() == 0) output = tr("Finished.");
    ui->outputEdit->append(git->highlightOutput(output));
    QTextCursor cursor = ui->outputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    ui->outputEdit->setTextCursor(cursor);
    ui->outputEdit->setFocus();
    gitTabRefreshRequested();
    if (command == GIT_COMMIT_COMMAND) {
        Editor * textEditor = getActiveEditor();
        if (textEditor != nullptr && textEditor->isReady()) {
            gitAnnotationRequested(textEditor->getFileName());
            gitDiffUnifiedRequested(textEditor->getFileName());
        }
    } else if (command == GIT_PULL_COMMAND || command == GIT_CLONE_COMMAND) {
        filebrowser->refreshFileBrowserDirectory(filebrowser->getRootPath());
    }
}

void MainWindow::gitTabRefreshRequested()
{
    gitBrowser->clear();
    QString dir = getGitWorkingDir();
    if (!Helper::folderExists(dir+"/"+GIT_DIRECTORY)) return;
    git->showStatusShort(getGitWorkingDir(), false, true);
}

void MainWindow::gitTabAddAndCommitRequested()
{
    on_actionGitCommit_triggered(true);
}

void MainWindow::gitTabAddRequested(QString path)
{
    if (path.size() == 0) return;
    QString fileName = getGitWorkingDir() + "/" + path;
    // no existence check
    git->addCurrent(getGitWorkingDir(), fileName);
}

void MainWindow::gitTabResetRequested(QString path)
{
    if (path.size() == 0) return;
    QString fileName = getGitWorkingDir() + "/" + path;
    // no existence check
    git->resetCurrent(getGitWorkingDir(), fileName);
}

void MainWindow::gitAnnotationRequested(QString path)
{
    QString dir = getGitWorkingDir();
    if (!Helper::folderExists(dir+"/"+GIT_DIRECTORY)) return;
    git->showAnnotation(getGitWorkingDir(), path, false, true);
}

void MainWindow::gitDiffUnifiedRequested(QString path)
{
    QString dir = getGitWorkingDir();
    if (!Helper::folderExists(dir+"/"+GIT_DIRECTORY)) return;
    git->showUncommittedDiffCurrentUnified(getGitWorkingDir(), path, false, true);
}

void MainWindow::installAndroidPackFinished(QString result)
{
    if (Settings::get("devpack_install_silent") == "yes") return;
    hideQAPanel();
    if (result.size() > 0) {
        if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
        ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_RESULTS_INDEX);
        ui->outputEdit->clear();
        ui->outputEdit->setHtml(result);
    }
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;
    Settings::change(dialog.getData());
    restartApp();
}

void MainWindow::on_actionHelpShortcuts_triggered()
{
    ShortcutsDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;
    Settings::change(dialog.getData());
    restartApp();
}

void MainWindow::on_actionHelpAbout_triggered()
{
    HelpDialog dialog(this);
    dialog.aboutContent();
    dialog.exec();
}

void MainWindow::on_actionHelpFAQ_triggered()
{
    HelpDialog dialog(this);
    dialog.faqContent();
    dialog.exec();
}

void MainWindow::on_actionHelpContact_triggered()
{
    QString url = "mailto:" + AUTHOR_EMAIL_USERNAME + "@" + AUTHOR_EMAIL_DOMAIN;
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::on_actionHelpDonate_triggered()
{
    QDesktopServices::openUrl(QUrl(AUTHOR_CARD_URL + "/" + AUTHOR_CARD_ID));
}

void MainWindow::on_actionHelpZiraCMS_triggered()
{
    QDesktopServices::openUrl(QUrl(AUTHOR_CMS_URL));
}

void MainWindow::on_actionHelpZiraDevPack_triggered()
{
    QDesktopServices::openUrl(QUrl(AUTHOR_DEVPACK_URL));
}

void MainWindow::resetLastSearchParams()
{
    lastSearchText = "";
    lastSearchExtensions = "";
    lastSearchExcludeDirs.clear();
    lastSearchOptionCase = false;
    lastSearchOptionWord = false;
    lastSearchOptionRegexp = false;

    ui->searchListWidget->clear();
}

void MainWindow::editorSearchInFilesRequested(QString text)
{
    if (text.size() == 0) text = lastSearchText;
    else lastSearchText = text;
    SearchDialog dialog(this);
    dialog.setText(text);
    if (project->isOpen()) {
        dialog.setDirectory(project->getPath());
    } else {
        dialog.setDirectory(filebrowser->getRootPath());
    }
    if (lastSearchExtensions.size() > 0) {
        dialog.setExtensions(lastSearchExtensions);
    } else {
        dialog.setExtensions(".*");
    }
    if (lastSearchExcludeDirs.size() > 0) {
        dialog.setExcludeDirs(lastSearchExcludeDirs);
    } else {
        dialog.clearExcludeDirs();
    }
    if (lastSearchOptionCase) {
        dialog.setCaseOption(true);
    } else {
        dialog.setCaseOption(false);
    }
    if (lastSearchOptionWord) {
        dialog.setWordOption(true);
    } else {
        dialog.setWordOption(false);
    }
    if (lastSearchOptionRegexp) {
        dialog.setRegexpOption(true);
    } else {
        dialog.setRegexpOption(false);
    }
    dialog.focusText();
    if (dialog.exec() != QDialog::Accepted) return;
    QString searchDirectory = dialog.getDirectory();
    QString searchText = dialog.getText();
    QString searchExtensions = dialog.getExtensions();
    bool searchOptionCase = dialog.getCaseOption();
    bool searchOptionWord = dialog.getWordOption();
    bool searchOptionRegexp = dialog.getRegexpOption();
    QStringList excludeDirs = dialog.getExcludeDirs();
    lastSearchText = searchText;
    lastSearchExtensions = searchExtensions;
    lastSearchExcludeDirs = excludeDirs;
    lastSearchOptionCase = searchOptionCase;
    lastSearchOptionWord = searchOptionWord;
    lastSearchOptionRegexp = searchOptionRegexp;
    if (searchDirectory.size() == 0 || searchText.size() == 0) return;
    if (!Helper::folderExists(searchDirectory)) return;
    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->searchListWidget->clear();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_SEARCH_INDEX);
    ui->outputTabWidget->setTabText(OUTPUT_TAB_SEARCH_INDEX, tr("Searching..."));
    setStatusBarText("Searching...");
    emit searchInFiles(searchDirectory, searchText, searchExtensions, searchOptionCase, searchOptionWord, searchOptionRegexp, excludeDirs);
}

void MainWindow::searchInFilesFound(QString file, QString lineText, int line, int symbol)
{
    if (lineText.size() > 300) lineText = lineText.mid(0, 300) + "...";
    QFileInfo fInfo = QFileInfo(file);
    QString text = lineText;
    if (file.size() > 0 && line > 0 && symbol >= 0) {
        text = Helper::intToStr(ui->searchListWidget->count()+1) + ". [" + fInfo.fileName() + ":" + Helper::intToStr(line) + "] " + lineText.trimmed();
    }
    QListWidgetItem * item = new QListWidgetItem();
    item->setText(text);
    item->setToolTip(file);
    item->setData(Qt::UserRole, QVariant(file));
    item->setData(Qt::UserRole+1, QVariant(line));
    item->setData(Qt::UserRole+2, QVariant(symbol));
    item->setForeground(searchResultsColor);
    item->setBackground(outputBgColor);
    ui->searchListWidget->addItem(item);
}

void MainWindow::searchInFilesFinished()
{
    ui->outputTabWidget->setTabText(OUTPUT_TAB_SEARCH_INDEX, tr("Search")+"("+Helper::intToStr(ui->searchListWidget->count())+")");
    if (ui->searchListWidget->count() == 0) {
        QListWidgetItem * item = new QListWidgetItem();
        item->setText(tr("No results"));
        item->setData(Qt::UserRole, QVariant(""));
        item->setData(Qt::UserRole+1, QVariant(-1));
        item->setData(Qt::UserRole+2, QVariant(-1));
        ui->searchListWidget->addItem(item);
    }
    setStatusBarText("Search finished");
}

void MainWindow::searchListItemDoubleClicked(QListWidgetItem *item)
{
    QString file = item->data(Qt::UserRole).toString();
    int line = item->data(Qt::UserRole+1).toInt();
    int symbol = item->data(Qt::UserRole+2).toInt();
    if (file.size() == 0 || line <= 0 || symbol < 0) return;
    if (!Helper::fileExists(file)) return;
    editorTabs->openFile(file);
    editorShowLineSymbol(line, symbol);
}

void MainWindow::sidebarProgressChanged(int v)
{
    if (v < 100 && !ui->sidebarProgressBarWrapperWidget->isVisible()) ui->sidebarProgressBarWrapperWidget->show();
    //if (v == 100 && ui->sidebarProgressBarWrapperWidget->isVisible()) ui->sidebarProgressBarWrapperWidget->hide();
    if (v >= 0 && v <= 100) ui->sidebarProgressBar->setValue(v);
}

void MainWindow::dragEnterEvent(QDragEnterEvent * event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent * event)
{
    foreach (const QUrl &url, event->mimeData()->urls()) {
        QString fileName = url.toLocalFile();
        editorTabs->openFile(fileName);
    }
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (tabsList->isVisible()) tabsList->hide();
    if (e->key() == Qt::Key_Escape) hideQAPanel();
    QMainWindow::keyPressEvent(e);
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    hideQAPanel();
    QMainWindow::mousePressEvent(e);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    hideQAPanel();
    #if defined(Q_OS_ANDROID)
    progressLine->updateGeometry(0, 0, geometry().width());
    #else
    progressLine->updateGeometry(ui->menuBar->geometry().x(), ui->menuBar->geometry().y() + ui->menuBar->geometry().height(), ui->menuBar->geometry().width());
    #endif
    progressInfo->updateGeometry(ui->statusBar->geometry().x(), ui->statusBar->geometry().y(), ui->statusBar->width(), ui->statusBar->height());
    QMainWindow::resizeEvent(event);
}

void MainWindow::showWelcomeScreen()
{
    editorsSplitter->hide();
    welcomeScreen->show();
    welcomeScreen->raise();
    welcomeScreen->focus();
}

void MainWindow::hideWelcomeScreen()
{
    welcomeScreen->hide();
    editorsSplitter->show();
    editorsSplitter->raise();
}

void MainWindow::editorFocused()
{
    isSplitActive = false;
    hideQAPanel();
    setStatusBarText(""); // update status bar
    Editor * editor = editorTabs->getActiveEditor();
    if (editor != nullptr) {
        setWindowModified(editor->isModified());
        setWindowTitleText(editor->getFileName());
        editorActionsChanged();
    } else {
        setWindowModified(false);
        setWindowTitleText(""); // update window title
    }
}

void MainWindow::editorSplitFocused()
{
    isSplitActive = true;
    hideQAPanel();
    setStatusBarText(""); // update status bar
    Editor * editor = editorTabsSplit->getActiveEditor();
    if (editor != nullptr) {
        setWindowModified(editor->isModified());
        setWindowTitleText(editor->getFileName());
        editorActionsChanged();
    } else {
        setWindowModified(false);
        setWindowTitleText(""); // update window title
    }
}

void MainWindow::showQAPanel()
{
    QRect rect = ui->centralWidget->geometry();
    int px = rect.x();
    int py = rect.y();
    int w = rect.width();
    int h = rect.height();
    qa->slideIn(px + w / 2, py + 1, w / 2, h - 2);
}

void MainWindow::showPopupText(QString text)
{
    QRect rect = ui->centralWidget->geometry();
    int px = rect.x();
    int py = rect.y();
    popup->displayText(px, py, text);
}

void MainWindow::showPopupError(QString text)
{
    QRect rect = ui->centralWidget->geometry();
    int px = rect.x();
    int py = rect.y();
    popup->displayError(px, py, text);
}

void MainWindow::hideQAPanel()
{
    tabsList->hide();
    if (!qa->isVisible()) return;
    qa->slideOut();
}

void MainWindow::quickAccessRequested(QString file, int line)
{
    if (file.size() > 0 && line > 0 && Helper::fileExists(file)) {
        hideQAPanel();
        editorTabs->openFile(file);
        Editor * editor = editorTabs->getActiveEditor();
        if (editor != nullptr && editor->getFileName() == file) {
            editor->gotoLine(line);
        }
    }
}

void MainWindow::quickFindRequested(QString text)
{

    QString dir = filebrowser->getRootPath();
    if (project->isOpen()) dir = project->getPath();
    WordsMapList words;
    QStringList wordPrefixes;
    if (project->isOpen()) {
        words.push_back(project->phpClassDeclarations);
        wordPrefixes.append("class: ");
        words.push_back(project->phpClassMethodDeclarations);
        wordPrefixes.append("method: ");
        words.push_back(project->phpFunctionDeclarations);
        wordPrefixes.append("function: ");
    }
    emit quickFind(dir, text, words, wordPrefixes);
}

void MainWindow::editorFilenameChanged(QString name)
{
    setWindowTitleText(name);
    QFileInfo fInfo(name);
    QString dir = fInfo.dir().absolutePath();
    if (dir.size() > 0) filebrowser->refreshFileBrowserDirectory(dir);
}

void MainWindow::editorTabOpened(int)
{
    hideWelcomeScreen();
    navigator->clear();
    enableActionsForOpenTabs();
    Editor * editor = editorTabs->getActiveEditor();
    if (editor != nullptr) {
        setWindowTitleText(editor->getFileName());
        clearMessagesTabText();
        editorActionsChanged();
    }
    updateTabsListButton();

    #if defined(Q_OS_ANDROID)
    if (QGuiApplication::primaryScreen()->primaryOrientation() == Qt::PortraitOrientation) {
        ui->sidebarDockWidget->hide();
    }
    #endif
}

void MainWindow::editorTabSplitOpened(int)
{
    hideWelcomeScreen();
    enableActionsForOpenTabs();
    Editor * editor = editorTabsSplit->getActiveEditor();
    if (editor != nullptr) {
        setWindowTitleText(editor->getFileName());
        editorActionsChanged();
    }
}

void MainWindow::editorTabSwitched(int)
{
    navigator->clear();
    clearMessagesTabText();
    setStatusBarText(""); // update status bar
    Editor * editor = editorTabs->getActiveEditor();
    if (editor != nullptr) {
        editor->setFocus();
        setWindowModified(editor->isModified());
        setWindowTitleText(editor->getFileName());
        parseTab();
        editorActionsChanged();
    } else {
        setWindowModified(false);
        setWindowTitleText(""); // update window title
    }
}

void MainWindow::editorTabSplitSwitched(int)
{
    setStatusBarText(""); // update status bar
    Editor * editor = editorTabsSplit->getActiveEditor();
    if (editor != nullptr) {
        editor->setFocus();
        setWindowModified(editor->isModified());
        setWindowTitleText(editor->getFileName());
        parseTabSplit();
        editorActionsChanged();
    } else {
        setWindowModified(false);
        setWindowTitleText(""); // update window title
    }
}

void MainWindow::editorTabClosed(int)
{
    Editor * editor = editorTabs->getActiveEditor();
    Editor * editorSplit = editorTabsSplit->getActiveEditor();
    if (editor == nullptr && editorSplit == nullptr) {
        disableActionsForEmptyTabs();
        showWelcomeScreen();
    }
    updateTabsListButton();
}

void MainWindow::editorTabSplitClosed(int index)
{
    Editor * editor = editorTabsSplit->getActiveEditor();
    if (editor == nullptr) {
        tabWidgetSplit->hide();
    }
    editorTabClosed(index);
}

void MainWindow::editorModifiedStateChanged(bool m)
{
    setWindowModified(m);
}

void MainWindow::editorSaved(int index)
{
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr || textEditor->getTabIndex() != index) return;

    Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
    if (textEditorSplit != nullptr && textEditorSplit->getFileName() == textEditor->getFileName()) {
        textEditorSplit->setFileIsOutdated();
    }

    parseTab();
    gitTabRefreshRequested();
    filesHistory[textEditor->getFileName()] = textEditor->getCursorLine();
}

void MainWindow::editorSplitSaved(int index)
{
    Editor * textEditorSplit = editorTabsSplit->getActiveEditor();
    if (textEditorSplit == nullptr || textEditorSplit->getTabIndex() != index) return;

    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor != nullptr && textEditor->getFileName() == textEditorSplit->getFileName()) {
        textEditor->setFileIsOutdated();
    }

    parseTabSplit();
    gitTabRefreshRequested();
    filesHistory[textEditorSplit->getFileName()] = textEditorSplit->getCursorLine();
}

void MainWindow::editorReady(int index)
{
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr || textEditor->getTabIndex() != index) return;
    parseTab();
}

void MainWindow::editorSplitReady(int index)
{
    Editor * textEditor = editorTabsSplit->getActiveEditor();
    if (textEditor == nullptr || textEditor->getTabIndex() != index) return;
    parseTabSplit();
}

QString MainWindow::getTmpDirPath()
{
    return QDir::tempPath();
}

void MainWindow::parseTab()
{
    if (tmpDisableParser) return;
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr) return;
    int tabIndex = textEditor->getTabIndex();
    QString path = textEditor->getFileName();
    std::string modeType = textEditor->getModeType();
    clearMessagesTabText();
    //if (modeType == MODE_UNKNOWN) return;
    if (modeType == MODE_MIXED) {
        if ((!project->isOpen() && parsePHPLintEnabled) || (project->isOpen() && project->isPHPLintEnabled())) emit parseLint(tabIndex, path);
        if (textEditor->isReady() && parsePHPEnabled) emit parseMixed(tabIndex, textEditor->getContent());
    }
    if (modeType == MODE_JS && textEditor->isReady() && parseJSEnabled) emit parseJS(tabIndex, textEditor->getContent());
    if (modeType == MODE_CSS && textEditor->isReady() && parseCSSEnabled) emit parseCSS(tabIndex, textEditor->getContent());
    if ((!project->isOpen() && parsePHPCSEnabled) || (project->isOpen() && project->isPHPCSEnabled())) emit parsePHPCS(tabIndex, path);
    if (gitCommandsEnabled && textEditor->isReady()) {
        gitAnnotationRequested(textEditor->getFileName());
        gitDiffUnifiedRequested(textEditor->getFileName());
    }
}

void MainWindow::parseTabSplit()
{
    if (tmpDisableParser) return;
    Editor * textEditor = editorTabsSplit->getActiveEditor();
    if (textEditor == nullptr) return;
    int tabIndex = textEditor->getTabIndex();
    QString path = textEditor->getFileName();
    std::string modeType = textEditor->getModeType();
    //if (modeType == MODE_UNKNOWN) return;
    if (modeType == MODE_MIXED) {
        if ((!project->isOpen() && parsePHPLintEnabled) || (project->isOpen() && project->isPHPLintEnabled())) emit parseLint(tabIndex, path);
    }
    if ((!project->isOpen() && parsePHPCSEnabled) || (project->isOpen() && project->isPHPCSEnabled())) emit parsePHPCS(tabIndex, path);
    if (gitCommandsEnabled && textEditor->isReady()) {
        gitAnnotationRequested(textEditor->getFileName());
        gitDiffUnifiedRequested(textEditor->getFileName());
    }
}

void MainWindow::parseLintFinished(int tabIndex, QStringList errorTexts, QStringList errorLines, QString output)
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    if (tabIndex != textEditor->getTabIndex()) return;
    textEditor->clearErrors();
    if (errorTexts.size() > 0 && errorTexts.size() == errorLines.size()) {
        for (int i=0; i<errorTexts.size(); i++) {
            QString lineStr = errorLines.at(i);
            QString errorStr = errorTexts.at(i);
            textEditor->setError(lineStr.toInt(), errorStr);
            if (editorTabs->getActiveEditor() == textEditor) addMessagesTabText(outputMsgErrorTpl.arg(lineStr).arg(errorStr));
        }
        textEditor->setParseError(true);
        textEditor->gotoLine(errorLines.at(0).toInt());
        textEditor->highlightErrorLine(errorLines.at(0).toInt());
    } else {
        textEditor->setParseError(false);
    }
    textEditor->updateMarksAndMapArea();
    if (errorTexts.size() > 0 && errorTexts.size() == errorLines.size()) {
        setStatusBarText(tr("PARSE ERROR"));
        if (editorTabs->getActiveEditor() == textEditor) scrollMessagesTabToTop();
    } else if (output.size() > 0 && (errorTexts.size() == 0 || errorTexts.size() != errorLines.size())) {
        setStatusBarText(tr("PARSE ERROR"));
        if (editorTabs->getActiveEditor() == textEditor) addMessagesTabText(outputMsgErrorTpl.arg("unknown").arg(output));
        if (editorTabs->getActiveEditor() == textEditor) scrollMessagesTabToTop();
    } else {
        setStatusBarText(tr("PARSE OK"));
    }
    if (errorTexts.size() > 0 && errorLines.size() > 0) {
        showPopupError("["+tr("Line")+": "+errorLines.at(0)+"] "+errorTexts.at(0));
    }
}

void MainWindow::parsePHPCSFinished(int tabIndex, QStringList errorTexts, QStringList errorLines)
{
    Editor * textEditor = getActiveEditor();
    if (textEditor == nullptr) return;
    if (tabIndex != textEditor->getTabIndex()) return;
    textEditor->clearWarnings();
    if (errorTexts.size() > 0 && errorTexts.size() == errorLines.size()) {
        for (int i=0; i<errorTexts.size(); i++) {
            QString lineStr = errorLines.at(i);
            QString errorStr = errorTexts.at(i);
            textEditor->setWarning(lineStr.toInt(), errorStr);
            if (editorTabs->getActiveEditor() == textEditor) addMessagesTabText(outputMsgWarningTpl.arg(lineStr).arg(errorStr));
        }
    }
    textEditor->updateMarksAndMapArea();
    if (editorTabs->getActiveEditor() == textEditor) scrollMessagesTabToTop();
}

void MainWindow::parseMixedFinished(int tabIndex, ParsePHP::ParseResult result)
{
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr) return;
    if (tabIndex != textEditor->getTabIndex()) return;
    if (!textEditor->getParseError()) {
        textEditor->clearErrors();
        if (result.errors.size()) {
            for (int i=0; i<result.errors.size(); i++) {
                ParsePHP::ParseResultError error = result.errors.at(i);
                textEditor->setError(error.line, error.text);
                textEditor->highlightError(error.symbol, 1);
            }
        }
        textEditor->updateMarksAndMapArea();
    }
    textEditor->setParseResult(result);
    navigator->build(result);
    qa->setParseResult(result, textEditor->getFileName());
}

void MainWindow::parseJSFinished(int tabIndex, ParseJS::ParseResult result)
{
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr) return;
    if (tabIndex != textEditor->getTabIndex()) return;
    textEditor->clearErrors();
    if (result.errors.size()) {
        for (int i=0; i<result.errors.size(); i++) {
            ParseJS::ParseResultError error = result.errors.at(i);
            textEditor->setError(error.line, error.text);
            textEditor->highlightError(error.symbol, 1);
        }
    }
    textEditor->updateMarksAndMapArea();
    textEditor->setParseResult(result);
    navigator->build(result);
    qa->setParseResult(result, textEditor->getFileName());
}

void MainWindow::parseCSSFinished(int tabIndex, ParseCSS::ParseResult result)
{
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr) return;
    if (tabIndex != textEditor->getTabIndex()) return;
    textEditor->clearErrors();
    if (result.errors.size()) {
        for (int i=0; i<result.errors.size(); i++) {
            ParseCSS::ParseResultError error = result.errors.at(i);
            textEditor->setError(error.line, error.text);
            textEditor->highlightError(error.symbol, 1);
        }
    }
    textEditor->updateMarksAndMapArea();
    textEditor->setParseResult(result);
    navigator->build(result);
    qa->setParseResult(result, textEditor->getFileName());
}

void MainWindow::parseProjectFinished(bool success, bool isModified)
{
    if (success) {
        setStatusBarText(tr("Loading project..."));
        reloadWords();
        project->loadWords();
    }
    setStatusBarText(tr(""));
    if (ui->sidebarProgressBarWrapperWidget->isVisible()) ui->sidebarProgressBarWrapperWidget->hide();
    editorTabs->initHighlighters();
    if (success && isModified) showPopupText(tr("Project '%1' updated").arg(project->getName()));
}

void MainWindow::projectCreateRequested(QString name, QString path, bool lintEnabled, bool csEnabled)
{
    if (!parsePHPEnabled) return;
    if (!project->create(name, path, lintEnabled, csEnabled, gitCommandsEnabled)) {
        Helper::showMessage(QObject::tr("Could not create the project."));
        return;
    }
    projectOpenRequested(path);
}

void MainWindow::projectEditRequested(QString name, QString path, bool lintEnabled, bool csEnabled)
{
    if (!project->edit(name, path, lintEnabled, csEnabled, editorTabs->getOpenTabFiles(), editorTabs->getOpenTabLines(), editorTabs->getCurrentTabIndex(), ui->todoEdit->toPlainText())) {
        Helper::showMessage(QObject::tr("Could not edit the project."));
        return;
    }
    clearMessagesTabText();
    setStatusBarText("Project saved.");
    Editor * editor = editorTabs->getActiveEditor();
    if (editor != nullptr) {
        setWindowModified(editor->isModified());
        setWindowTitleText(editor->getFileName());
        parseTab();
    } else {
        setWindowModified(false);
        setWindowTitleText(""); // update window title
    }
}

void MainWindow::projectOpenRequested(QString path)
{
    if (!parsePHPEnabled) return;
    if (!project->exists(path)) {
        Helper::showMessage(QObject::tr("Project not found in this directory."));
        return;
    }
    project->save(editorTabs->getOpenTabFiles(), editorTabs->getOpenTabLines(), editorTabs->getCurrentTabIndex(), ui->todoEdit->toPlainText());
    closeAllTabsRequested();
    if (!project->open(path)) {
        Helper::showMessage(QObject::tr("Could not open the project."));
        return;
    }
    args.clear();
    filebrowser->rebuildFileBrowserTree(path);
    ui->outputEdit->clear();
    resetLastSearchParams();
    enableActionsForOpenProject();
    setStatusBarText(tr("Scanning project..."));
    emit parseProject(project->getPath());
    gitTabRefreshRequested();
    if (terminal != nullptr) {
        terminal->changeDir(project->getPath());
    }
}

void MainWindow::openTabsRequested(QStringList files, bool initHighlight)
{
    for (int i=0; i<files.size(); i++) {
        QString file = files.at(i);
        editorTabs->openFile(file, initHighlight);
    }
}

void MainWindow::gotoTabLinesRequested(QList<int> lines)
{
    editorTabs->setTabLines(lines);
}

void MainWindow::switchToTabRequested(int index)
{
    editorTabs->setActiveTab(index);
}

void MainWindow::showTodoRequested(QString text)
{
    ui->todoEdit->setPlainText(text);
}

void MainWindow::closeAllTabsRequested()
{
    tmpDisableParser = true;
    editorTabs->closeSaved();
    tmpDisableParser = false;
}

void MainWindow::reloadWords()
{
    HighlightWords::reset();
    CompleteWords::reset();
    HelpWords::reset();

    HighlightWords::load();
    CompleteWords::load();
    HelpWords::load();
}

void MainWindow::editorShowDeclaration(QString name)
{
    if (name.size() > 0 && name.at(0) == "\\") name = name.mid(1);
    if (name.size() == 0) return;
    QString path = "";
    int line = 0;
    project->findDeclaration(name, path, line);
    if (path.size() > 0 && line > 0 && Helper::fileExists(path)) {
        editorTabs->openFile(path);
        Editor * editor = editorTabs->getActiveEditor();
        if (editor != nullptr && editor->getFileName() == path) {
            editor->gotoLine(line);
        }
    }
}

void MainWindow::editorShowHelp(QString name)
{
    if (name.size() > 0 && name.at(0) == "\\") name = name.mid(1);
    if (name.size() == 0) return;
    QString php_manual_path = QString::fromStdString(Settings::get("php_manual_path"));
    if (php_manual_path.size() == 0) {
        QDir phpManDir = QDir("./"+PHP_MANUAL_FALLBACK_FOLDER);
        php_manual_path = phpManDir.absolutePath();
        if (!Helper::folderExists(php_manual_path)) php_manual_path = "";
    }
    bool phpManualIsInstalled = false;
    if (php_manual_path.size() > 0 && Helper::folderExists(php_manual_path)) phpManualIsInstalled = true;
    QString file = HelpWords::findHelpFile(name);
    if (phpManualIsInstalled && file.size() > 0 && Helper::fileExists(php_manual_path + "/" + file)) {
        setHelpTabSource(php_manual_path + "/" + file);
    } else if (!phpManualIsInstalled && file.size() > 0) {
        QString phpURL = "https://www.php.net/manual/" + file.replace(QRegularExpression(".html$"), ".php");
        QString helpStr = tr("PHP Manual is not installed. Go to %1").arg("<a href=\""+phpURL+"\">"+phpURL+"</a>");
        setHelpTabContents(helpStr);
    } else {
        HelpWords * helpWords = &HelpWords::instance();
        clearHelpTabSource();
        QString text = "";
        if (name.indexOf("::") > 0) {
            helpWords->phpClassMethodDescsIterator = helpWords->phpClassMethodDescs.find(name.toStdString());
            if (helpWords->phpClassMethodDescsIterator != helpWords->phpClassMethodDescs.end()) {
                text = QString::fromStdString(helpWords->phpClassMethodDescsIterator->second);
            }
        } else {
            helpWords->phpFunctionDescsIterator = helpWords->phpFunctionDescs.find(name.toStdString());
            if (helpWords->phpFunctionDescsIterator != helpWords->phpFunctionDescs.end()) {
                text = QString::fromStdString(helpWords->phpFunctionDescsIterator->second);
            }
        }
        if (text.size() > 0) {
            text = text.replace("<", "&lt;").replace(">", "&gt;");
            text = text.replace("\n", "<br />");
            ui->helpBrowser->setHtml("<h1>"+name+"</h1>"+text);
            if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
            ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_HELP_INDEX);
        }
    }
}

void MainWindow::editorParsePHPRequested(int index, QString text)
{
    if (!parsePHPEnabled) return;
    emit parseMixed(index, text);
}

void MainWindow::editorParseJSRequested(int index, QString text)
{
    if (!parseJSEnabled) return;
    emit parseJS(index, text);
}

void MainWindow::editorParseCSSRequested(int index, QString text)
{
    if (!parseCSSEnabled) return;
    emit parseCSS(index, text);
}

void MainWindow::clearMessagesTabText()
{
    ui->messagesBrowser->setText("");
    QString msgTabText = ui->outputTabWidget->tabText(OUTPUT_TAB_MESSAGES_INDEX);
    if (msgTabText.size()>1) {
        msgTabText.replace(QRegularExpression("[(].*[)]"), "");
    }
    ui->outputTabWidget->setTabText(OUTPUT_TAB_MESSAGES_INDEX, msgTabText);
    outputMsgCount = 0;
}

void MainWindow::addMessagesTabText(QString text)
{
    if (text.size() == 0) return;
    outputMsgCount++;
    ui->messagesBrowser->append(text);
    QString msgTabText = ui->outputTabWidget->tabText(OUTPUT_TAB_MESSAGES_INDEX);
    if (msgTabText.size()>1) {
        msgTabText.replace(QRegularExpression("[(].*[)]"), "");
    }
    msgTabText += "("+Helper::intToStr(outputMsgCount)+")";
    ui->outputTabWidget->setTabText(OUTPUT_TAB_MESSAGES_INDEX, msgTabText);
}

void MainWindow::clearHelpTabSource()
{
    ui->helpBrowser->clear();
}

void MainWindow::setHelpTabSource(QString path)
{
    if (path.size() == 0 || !Helper::fileExists(path)) return;
    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_HELP_INDEX);
    QString source = Helper::loadTextFile(path, PHP_MANUAL_ENCODING, PHP_MANUAL_ENCODING, true);
    source.replace(phpManualHeaderExpr, "");
    source.replace(phpManualBreadcrumbsExpr, "");
    ui->helpBrowser->setHtml(source);
}

void MainWindow::setHelpTabContents(QString html)
{
    if (html.size() == 0) return;
    hideQAPanel();
    if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
    ui->outputTabWidget->setCurrentIndex(OUTPUT_TAB_HELP_INDEX);
    ui->helpBrowser->setHtml(html);
}

void MainWindow::helpBrowserAnchorClicked(QUrl url)
{
    QString file = url.toString();
    if (file.indexOf("#") >= 0) file = file.mid(0, file.indexOf("#"));
    if (file.size() == 0) return;
    if (file.indexOf("https://") == 0 || file.indexOf("http://") == 0) {
        QDesktopServices::openUrl(url);
    } else {
        QString php_manual_path = QString::fromStdString(Settings::get("php_manual_path"));
        if (php_manual_path.size() == 0 || !Helper::folderExists(php_manual_path)) return;
        if (!Helper::fileExists(php_manual_path + "/" + file)) return;
        setHelpTabSource(php_manual_path + "/" + file);
    }
}

void MainWindow::messagesBrowserAnchorClicked(QUrl url)
{
    QString lineStr = url.toString();
    int line = lineStr.toInt();
    if (line <= 0) return;
    editorShowLine(line);
}

void MainWindow::scrollMessagesTabToTop()
{
    QTextCursor textCursor = ui->messagesBrowser->textCursor();
    textCursor.movePosition(QTextCursor::Start);
    ui->messagesBrowser->setTextCursor(textCursor);
}

void MainWindow::editorShowLine(int line)
{
    if (line <= 0) return;
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->gotoLine(line);
}

void MainWindow::editorShowLineSymbol(int line, int symbol)
{
    if (line <= 0 || symbol < 0) return;
    Editor * textEditor = editorTabs->getActiveEditor();
    if (textEditor == nullptr) return;
    textEditor->gotoLineSymbol(line, symbol);
}

void MainWindow::setWindowTitleText(QString text)
{
    QString title = APPLICATION_NAME;
    QString projectTitle = project->getName();
    if (projectTitle.size() > 0) title = projectTitle;
    if (text.size() > 0) title += " - " + text;
    setWindowTitle(title + " [*]");
}

void MainWindow::setStatusBarText(QString text)
{
    Editor * editor = getActiveEditor();
    if (editor == nullptr) {
        statusBar()->showMessage("");
        return;
    }
    int blocksCount = editor->document()->blockCount();
    QTextCursor curs = editor->textCursor();
    int blockNumber = curs.block().blockNumber();
    int cursPosInBlock = curs.positionInBlock();
    std::string tabType = editor->getTabType();
    int tabWidth = editor->getTabWidth();
    QString tabMode = QString::fromStdString(tabType).toUpper()+" : "+Helper::intToStr(tabWidth);
    QString newLineMode = QString::fromStdString(editor->getNewLineMode()).toUpper();
    QString encoding = QString::fromStdString(editor->getEncoding()).toUpper();
    QString overwriteMode = editor->isOverwrite() ? "OVERWRITE" : "INSERT";

    QString separator = "    |    ";
    if (text.size()>0) text = separator + text;
    statusBar()->showMessage(
                tr("Line") + ": " + Helper::intToStr(blockNumber+1)+" / "+Helper::intToStr(blocksCount) +
                separator + tr("Column") + ": " + Helper::intToStr(cursPosInBlock+1) +
                separator + tabMode +
                separator + newLineMode +
                separator + encoding +
                separator + overwriteMode +
                text
            );
}

void MainWindow::toolbarOrientationChanged(Qt::Orientation /*orientation*/)
{
    /*
    if (orientation == Qt::Vertical) {
        ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    } else {
        ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    }
    */
}

void MainWindow::sidebarActionTriggered(bool /*checked*/)
{
    if (ui->sidebarDockWidget->isVisible()) {
        ui->sidebarDockWidget->hide();
    } else {
        ui->sidebarDockWidget->show();
    }
}

void MainWindow::outputActionTriggered(bool /*checked*/)
{
    if (ui->outputDockWidget->isVisible()) {
        ui->outputDockWidget->hide();
    } else {
        ui->outputDockWidget->show();
    }
}

void MainWindow::outputDockLocationChanged(Qt::DockWidgetArea area)
{
    if (area == Qt::RightDockWidgetArea) {
        ui->outputTabWidget->setTabPosition(QTabWidget::East);
    } else if (area == Qt::LeftDockWidgetArea) {
        ui->outputTabWidget->setTabPosition(QTabWidget::West);
    } else {
        ui->outputTabWidget->setTabPosition(QTabWidget::North);
    }

    QDockWidget::DockWidgetFeatures features = ui->outputDockWidget->features();
    if (area == Qt::RightDockWidgetArea && (features & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->outputDockWidget->setFeatures(features ^ QDockWidget::DockWidgetVerticalTitleBar);
    } else if (area != Qt::RightDockWidgetArea && !(features & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->outputDockWidget->setFeatures(features | QDockWidget::DockWidgetVerticalTitleBar);
    }
}

void MainWindow::sidebarDockLocationChanged(Qt::DockWidgetArea area)
{
    if (area == Qt::RightDockWidgetArea) {
        ui->sidebarTabWidget->setTabPosition(QTabWidget::East);
    } else {
        ui->sidebarTabWidget->setTabPosition(QTabWidget::West);
    }

    QDockWidget::DockWidgetFeatures features = ui->sidebarDockWidget->features();
    if (area == Qt::RightDockWidgetArea && (features & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->sidebarDockWidget->setFeatures(features ^ QDockWidget::DockWidgetVerticalTitleBar);
    } else if (area != Qt::RightDockWidgetArea && !(features & QDockWidget::DockWidgetVerticalTitleBar)) {
        ui->sidebarDockWidget->setFeatures(features | QDockWidget::DockWidgetVerticalTitleBar);
    }
}

void MainWindow::workerMessage(QString text)
{
    Helper::showMessage(text);
}

void MainWindow::restartApp()
{
    if (Helper::showQuestion(tr("Restart required"), tr("Some changes will take effect after restart. Restart now ?"))
        //QMessageBox::question(this, tr("Restart required"), tr("Some changes will take effect after restart. Restart now ?"), QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok
    ) {
        MainWindow::WANT_RESTART = true;
        if (close()) {
            QApplication::exit();
        } else {
            MainWindow::WANT_RESTART = false;
        }
    }
}

void MainWindow::applyThemeColors(QString pluginsDir, bool light, bool applyFont)
{
    QString style = "";

    // setting widgets font
    if (applyFont) {
        QFont font = QApplication::font();
        style += "QMenu, QTreeWidget, QTabBar::tab, QLineEdit, QPushButton, QLabel, QCheckBox, QRadioButton, QComboBox, QDockWidget::title, QListWidget, QTreeView, QListView, QSidebar {font: "+Helper::intToStr(font.pointSize())+"pt \""+font.family()+"\";}" + "\n";
    }

    if (theme == THEME_DARK) {
        QFile f(":/styles/dark/style");
        f.open(QIODevice::ReadOnly);
        QTextStream in(&f);
        style += in.readAll() + "\n";
        f.close();
    } else if (theme == THEME_LIGHT) {
        QFile f(":/styles/light/style");
        f.open(QIODevice::ReadOnly);
        QTextStream in(&f);
        style += in.readAll() + "\n";
        f.close();
    } else if (customThemesPath.size() > 0 && Helper::fileExists(customThemesPath + "/" + theme + "/" + CUSTOM_THEME_CSS_FILE) && theme.indexOf(STYLE_PLUGIN_DISPLAY_NAME_SUFFIX) < 0) {
        QFile f(customThemesPath + "/" + theme + "/" + CUSTOM_THEME_CSS_FILE);
        f.open(QIODevice::ReadOnly);
        QTextStream in(&f);
        style += in.readAll().replace("$theme_dir", customThemesPath + "/" + theme) + "\n";
        f.close();
    } else if (theme.indexOf(STYLE_PLUGIN_DISPLAY_NAME_SUFFIX) > 0) {
        QStringList stylePlugins = Helper::getInstalledStylePlugins(pluginsDir);
        QString stylePlugin = theme.mid(0, theme.size() - STYLE_PLUGIN_DISPLAY_NAME_SUFFIX.size());
        if (stylePlugins.contains(stylePlugin)) {
            Helper::loadStylePlugin(stylePlugin, pluginsDir, light);
        }
        return; // do not load scheme for style plugins
    }

    if (colorSheme == COLOR_SCHEME_DARK) {
        QFile f(":/styles/dark/scheme");
        f.open(QIODevice::ReadOnly);
        QTextStream in(&f);
        style += in.readAll() + "\n";
        f.close();
    } else if (colorSheme == COLOR_SCHEME_LIGHT || customThemesPath.size() == 0 || !Helper::fileExists(customThemesPath + "/" + colorSheme + "/" + CUSTOM_THEME_SCHEME_FILE)) {
        QFile f(":/styles/light/scheme");
        f.open(QIODevice::ReadOnly);
        QTextStream in(&f);
        style += in.readAll() + "\n";
        f.close();
    } else if (customThemesPath.size() > 0 && Helper::fileExists(customThemesPath + "/" + colorSheme + "/" + CUSTOM_THEME_SCHEME_FILE)) {
        QFile f(customThemesPath + "/" + colorSheme + "/" + CUSTOM_THEME_SCHEME_FILE);
        f.open(QIODevice::ReadOnly);
        QTextStream in(&f);
        style += in.readAll() + "\n";
        f.close();
    }

    if (style.trimmed().size() > 0) setStyleSheet(style);
}

void MainWindow::applyThemeIcons()
{
    Icon::reset();
    if (theme == THEME_DARK) {
        Icon::applyActionIcons(ui->menuBar, ":/styles/dark/icons");
    } else if (theme == THEME_LIGHT) {
        Icon::applyActionIcons(ui->menuBar, ":/styles/light/icons");
    } else if (customThemesPath.size() > 0 && Helper::folderExists(customThemesPath + "/" + theme + "/" + CUSTOM_THEME_ICONS_FOLDER) && theme.indexOf(STYLE_PLUGIN_DISPLAY_NAME_SUFFIX) < 0) {
        Icon::applyActionIcons(ui->menuBar, customThemesPath + "/" + theme + "/" + CUSTOM_THEME_ICONS_FOLDER);
    }

    QIcon gitPullIcon = Icon::get("actionGitPull");
    if (!gitPullIcon.isNull()) ui->gitTabPullButton->setIcon(gitPullIcon);
    QIcon gitPushIcon = Icon::get("actionGitPush");
    if (!gitPushIcon.isNull()) ui->gitTabPushButton->setIcon(gitPushIcon);
    QIcon gitCommitIcon = Icon::get("actionGitCommit");
    if (!gitCommitIcon.isNull()) ui->gitTabCommitButton->setIcon(gitCommitIcon);
    QIcon gitRefreshIcon = Icon::get("actionRefresh");
    if (!gitRefreshIcon.isNull()) ui->gitTabRefreshButton->setIcon(gitRefreshIcon);
}

void MainWindow::activateProgressLine()
{
    progressLine->activate();
}

void MainWindow::deactivateProgressLine()
{
    progressLine->deactivate();
}

void MainWindow::activateProgressInfo(QString text)
{
    progressInfo->setText(text);
    progressInfo->activate();
}

void MainWindow::deactivateProgressInfo()
{
    progressInfo->deactivate();
}

void MainWindow::updateProgressInfo(QString text)
{
    progressInfo->setText(text);
}

void MainWindow::updateTabsListButton()
{
    if (ui->tabWidget->count() > 1) tabsListButton->show();
    else tabsListButton->hide();
    if (tabsListButton->isVisible()) {
        tabsListButton->setGeometry(ui->tabWidget->width()-ui->tabWidget->tabBar()->height(), 0, ui->tabWidget->tabBar()->height(), ui->tabWidget->tabBar()->height());
        ui->tabWidget->tabBar()->setGeometry(ui->tabWidget->tabBar()->x(), ui->tabWidget->tabBar()->y(), ui->tabWidget->width()-ui->tabWidget->tabBar()->height(), ui->tabWidget->tabBar()->height());
    }
}

void MainWindow::editorTabsResize()
{
    updateTabsListButton();
}

void MainWindow::tabsListTriggered()
{
    if (!tabsList->isVisible() && ui->tabWidget->count() > 0) {
        tabsList->clear();
        for (int i=0; i<ui->tabWidget->count(); i++) {
            tabsList->addItem(ui->tabWidget->tabText(i), ui->tabWidget->tabToolTip(i), i);
        }
        tabsList->show();
        tabsList->raise();
        tabsList->setFocus();
        tabsList->setCurrentRow(ui->tabWidget->currentIndex());

        QRect editorTabsRectM = editorTabs->getGeometryMappedTo(this);
        int rowCo = tabsList->model()->rowCount();
        int width = tabsList->sizeHintForColumn(0) + tabsList->frameWidth() * 2;
        width += 100; // right margin
        int height = rowCo * tabsList->sizeHintForRow(0) + tabsList->frameWidth() * 2;
        QRect listRect = tabsList->geometry();
        listRect.setX(editorTabsRectM.x()+ui->tabWidget->width() - width);
        listRect.setY(editorTabsRectM.y()+ui->tabWidget->tabBar()->height());
        listRect.setWidth(width);
        listRect.setHeight(height);
        tabsList->setGeometry(listRect);
    } else {
        tabsList->hide();
    }
}

void MainWindow::tabsListSelected(int index)
{
    if (index < 0) return;
    if (index >= ui->tabWidget->count()) return;
    editorTabs->setActiveTab(index);
}

void MainWindow::showTerminal()
{
    if (terminal == nullptr) return;
    if (terminalTabIndex < 0) return;
    hideQAPanel();
    if (!ui->outputDockWidget->isVisible() || !terminal->getWidget()->hasFocus()) {
        if (!ui->outputDockWidget->isVisible()) ui->outputDockWidget->show();
        ui->outputTabWidget->setCurrentIndex(terminalTabIndex);
        terminal->getWidget()->setFocus();
    } else {
        ui->outputDockWidget->hide();
    }
}

void MainWindow::terminalCopy()
{
    if (terminal == nullptr) return;
    terminal->copy();
}

void MainWindow::terminalPaste()
{
    if (terminal == nullptr) return;
    terminal->paste();
}

void MainWindow::startTerminal()
{
    if (terminal == nullptr) return;
    terminal->startShell();
}

void MainWindow::inputMethodVisibleChanged()
{
    if (QApplication::inputMethod()->isVisible()) {
        ui->statusBar->setVisible(false);
        ui->tabWidget->tabBar()->setVisible(false);
        tabWidgetSplit->tabBar()->setVisible(false);
        tabsListButton->hide();

        QTimer::singleShot(INPUT_METHOD_ENSURE_CURSOR_VISIBLE_DELAY, this, [this](){
            Editor * textEditor = getActiveEditor();
            if (textEditor != nullptr) {
                textEditor->ensureCursorVisible();
            }
        });
    } else {
        ui->statusBar->setVisible(true);
        ui->tabWidget->tabBar()->setVisible(true);
        tabWidgetSplit->tabBar()->setVisible(true);
        updateTabsListButton();
    }
}

void MainWindow::applicationStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationActive) {
        if (suspended) {
            emit initWorker();
        }
        suspended = false;
    } else if (state == Qt::ApplicationSuspended) {
        if (progressInfo->isVisible()) {
            emit progressInfo->cancelTriggered();
        }
        saveStateBeforeExit();
        suspended = true;
    }
}
