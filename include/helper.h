/*******************************************
 * Zira Editor
 * A lightweight PHP Editor
 * (C)2019 https://github.com/ziracms/editor
 *******************************************/

#ifndef HELPER_H
#define HELPER_H

#include <QList>
#include <QMenu>
#include <QLineEdit>
#include "plugininterface.h"
#include "spellcheckerinterface.h"
#include "terminalinterface.h"
#include "contextdialog.h"

extern const QString APPLICATION_NAME;
extern const QString APPLICATION_VERSION;
extern const QString ORGANIZATION_NAME;
extern const QString PROJECT_NAME;
extern const QString AUTHOR_EMAIL_USERNAME;
extern const QString AUTHOR_EMAIL_DOMAIN;
extern const QString AUTHOR_CARD_URL;
extern const QString AUTHOR_CARD_ID;
extern const QString AUTHOR_CMS_URL;
extern const QString AUTHOR_DEVPACK_URL;
extern const QString GITHUB_EDITOR_URL;

extern const QString STYLE_PLUGIN_SUFFIX;
extern const QString STYLE_PLUGIN_DISPLAY_NAME_SUFFIX;
extern const QString DIALOG_HEADER_STYLESHEET;

extern const int ANDROID_DIALOG_ANIMATION_DURATION;

class Helper
{
public:
    static QString loadFile(QString path, std::string encoding, std::string fallbackEncoding, bool silent = false);
    static QString loadTextFile(QString path, std::string encoding, std::string fallbackEncoding, bool silent = false);
    static bool saveTextFile(QString path, const QString & text, std::string encoding);
    static bool createFile(QString path);
    static bool createDir(QString path);
    static bool deleteFile(QString path);
    static bool deleteFolder(QString path);
    static bool deleteFolderRecursivly(QString startDir);
    static bool renameFile(QString path, QString newpath);
    static bool renameDir(QString path, QString newpath);
    static bool renameFileOrFolder(QString path, QString newpath);
    static bool copyFile(QString path, QString newpath);
    static bool fileExists(QString path);
    static bool folderExists(QString path);
    static bool fileOrFolderExists(QString path);
    static qint64 getFileSize(QString path);
    static QString intToStr(int n);
    static QString doubleToStr(double n);
    static QString stripScopedText(QString scopedText);
    static void log(int n);
    static void log(QString str);
    static void log(const char * str);
    static void log(std::string str);
    static QString getPluginFile(QString name, QString path);
    static QObject * loadPlugin(QString name, QString path = "");
    static bool isPluginExists(QString name, QString path = "");
    static SpellCheckerInterface * loadSpellChecker(QString path = "");
    static bool loadStylePlugin(QString name, QString path, bool light = false);
    static TerminalInterface * loadTerminalPlugin(QString path = "");
    static QStringList getInstalledStylePlugins(QString path);
    static QString getExistingDirectory(QWidget * parent, QString title, QString directory = "");
    static QString getExistingFile(QWidget * parent, QString title, QString directory = "", QString filter= "");
    static QString getSaveFileName(QWidget * parent, QString title, QString directory = "", QString filter= "");
    static QWidget * getWindowWidget();
    static void showMessage(QString text);
    static bool showQuestion(QString title, QString msg);
    static QString showInputDialog(QString title, QString label, QLineEdit::EchoMode mode, QString defaultValue = "", QString description = "");
    static bool isQtVersionLessThan(int maj, int min, int mic);
    static void setApplicationAttributes();
    static QAction * contextMenuToDialog(QMenu * menu, QWidget * parent = nullptr);
    #if defined(Q_OS_ANDROID)
    static void requestAndroidPermissions();
    #endif
};

#endif // HELPER_H
