/*******************************************
 * Zira Editor
 * A lightweight PHP Editor
 * (C)2019 https://github.com/ziracms/editor
 *******************************************/

#include "completewords.h"

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QTimer>

const int LOAD_DELAY = 250; // should not be less then PROJECT_LOAD_DELAY

CompleteWords::CompleteWords(){}

CompleteWords& CompleteWords::instance()
{
    static CompleteWords _instance;
    return _instance;
}

void CompleteWords::loadDelayed()
{
    instance()._loadDelayed();
}

void CompleteWords::_loadDelayed()
{
    QTimer::singleShot(LOAD_DELAY, this, SLOT(load()));
}

void CompleteWords::load()
{
    instance()._load();
}

void CompleteWords::_load()
{
    loadCSSWords();
    loadHTMLWords();
    loadJSWords();
    loadFlutterWords();
    loadPHPWords();
}

void CompleteWords::reload()
{
    instance()._reload();
}

void CompleteWords::_reload()
{
    reset();
    load();
}

void CompleteWords::reset()
{
    instance()._reset();
}

void CompleteWords::_reset()
{
    tooltipsPHP.clear();
    htmlTags.clear();
    phpFunctionTypes.clear();
    phpClassMethodTypes.clear();
    phpClassParents.clear();
    htmlAllTagsComplete.clear();
    cssPropertiesComplete.clear();
    cssPseudoComplete.clear();
    cssValuesComplete.clear();
    jsObjectsComplete.clear();
    jsSpecialsComplete.clear();
    jsFunctionsComplete.clear();
    jsInterfacesComplete.clear();
    jsMethodsComplete.clear();
    jsEventsComplete.clear();
    phpFunctionsComplete.clear();
    phpConstsComplete.clear();
    phpClassesComplete.clear();
    phpClassConstsComplete.clear();
    phpClassPropsComplete.clear();
    phpClassMethodsComplete.clear();
    phpGlobalsComplete.clear();
    phpSpecialsComplete.clear();
}

void CompleteWords::loadCSSWords()
{
    QString k;

    // css properties
    QFile pf(":/syntax/css_props");
    pf.open(QIODevice::ReadOnly);
    QTextStream pin(&pf);
    while (!pin.atEnd()) {
        k = pin.readLine();
        if (k == "") continue;
        cssPropertiesComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addCSSProperty(k);
    }
    pf.close();

    // css pseudo
    QFile psf(":/syntax/css_pseudo");
    psf.open(QIODevice::ReadOnly);
    QTextStream psin(&psf);
    while (!psin.atEnd()) {
        k = psin.readLine();
        if (k == "") continue;
        cssPseudoComplete[k.toStdString()] = k.toStdString();
    }
    psf.close();

    // css values
    QFile vsf(":/syntax/css_values");
    vsf.open(QIODevice::ReadOnly);
    QTextStream vsin(&vsf);
    while (!vsin.atEnd()) {
        k = vsin.readLine();
        if (k == "") continue;
        cssValuesComplete[k.toStdString()] = k.toStdString();
    }
    vsf.close();
}

void CompleteWords::loadHTMLWords()
{
    QString k;

    // html tags (without short tags)
    QFile tf(":/syntax/html_tags");
    tf.open(QIODevice::ReadOnly);
    QTextStream tin(&tf);
    while (!tin.atEnd()) {
        k = tin.readLine();
        if (k == "") continue;
        htmlTags[k.toStdString()] = k.toStdString();
    }
    tf.close();

    // all html tags
    QFile sf(":/syntax/html_alltags");
    sf.open(QIODevice::ReadOnly);
    QTextStream sin(&sf);
    while (!sin.atEnd()) {
        k = sin.readLine();
        if (k == "") continue;
        htmlAllTagsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addHTMLTag(k);
    }
    sf.close();

    // html short tags
    QFile cf(":/syntax/html_shortags");
    cf.open(QIODevice::ReadOnly);
    QTextStream cin(&cf);
    while (!cin.atEnd()) {
        k = cin.readLine();
        if (k == "") continue;
        HighlightWords::addHTMLShortTag(k);
    }
    cf.close();
}

void CompleteWords::loadJSWords()
{
    QString k;

    // js built-in objects
    QFile of(":/syntax/js_objects");
    of.open(QIODevice::ReadOnly);
    QTextStream oin(&of);
    while (!oin.atEnd()) {
        k = oin.readLine();
        if (k == "") continue;
        jsObjectsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addJSObject(k);
    }
    of.close();

    // js specials
    QFile sf(":/syntax/js_specials");
    sf.open(QIODevice::ReadOnly);
    QTextStream sin(&sf);
    while (!sin.atEnd()) {
        k = sin.readLine();
        if (k == "") continue;
        jsSpecialsComplete[k.toStdString()] = k.toStdString();
    }
    sf.close();

    // js functions
    QFile ff(":/syntax/js_functions");
    ff.open(QIODevice::ReadOnly);
    QTextStream fin(&ff);
    while (!fin.atEnd()) {
        k = fin.readLine();
        if (k == "") continue;
        QString kName = "", kParams = "";
        int kSep = k.indexOf("(");
        if (kSep > 0) {
            kName = k.mid(0, kSep).trimmed();
            kParams = k.mid(kSep).trimmed();
            jsFunctionsComplete[kName.toStdString()] = kParams.toStdString();
            HighlightWords::addJSFunction(kName);
        } else {
            jsFunctionsComplete[k.toStdString()] = k.toStdString();
            HighlightWords::addJSFunction(k);
        }
    }
    ff.close();

    // js interfaces
    QFile inf(":/syntax/js_interfaces");
    inf.open(QIODevice::ReadOnly);
    QTextStream inin(&inf);
    while (!inin.atEnd()) {
        k = inin.readLine();
        if (k == "") continue;
        jsInterfacesComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addJSInterface(k);
    }
    inf.close();

    // js methods
    QFile mf(":/syntax/js_methods");
    mf.open(QIODevice::ReadOnly);
    QTextStream min(&mf);
    while (!min.atEnd()) {
        k = min.readLine();
        if (k == "") continue;
        QString kName = "", kParams = "";
        int kSep = k.indexOf("(");
        if (kSep > 0) {
            kName = k.mid(0, kSep).trimmed();
            kParams = k.mid(kSep).trimmed();
            jsMethodsComplete[kName.toStdString()] = kParams.toStdString();
        } else {
            jsMethodsComplete[k.toStdString()] = k.toStdString();
        }
    }
    mf.close();

    // js events
    QFile ef(":/syntax/js_events");
    ef.open(QIODevice::ReadOnly);
    QTextStream ein(&ef);
    while (!ein.atEnd()) {
        k = ein.readLine();
        if (k == "") continue;
        jsEventsComplete[k.toStdString()] = k.toStdString();
    }
    ef.close();
}

void CompleteWords::loadFlutterWords()
{
    QString k;

    // dart core
    QFile dcr(":/syntax/dart_core");
    dcr.open(QIODevice::ReadOnly);
    QTextStream dcrin(&dcr);
    while (!dcrin.atEnd()) {
        k = dcrin.readLine();
        if (k == "") continue;
        dartObjectsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addJSExtDartObject(k);
    }
    dcr.close();

    // dart core functions
    QFile dcf(":/syntax/dart_functions");
    dcf.open(QIODevice::ReadOnly);
    QTextStream dcfin(&dcf);
    while (!dcfin.atEnd()) {
        k = dcfin.readLine();
        if (k == "") continue;
        dartFunctionsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addJSExtDartFunction(k);
    }
    dcf.close();

    // flutter classes
    QFile flc(":/syntax/flutter_classes");
    flc.open(QIODevice::ReadOnly);
    QTextStream flcin(&flc);
    while (!flcin.atEnd()) {
        k = flcin.readLine();
        if (k == "") continue;
        flutterObjectsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addJSExtDartObject(k);
    }
    flc.close();

    // flutter widgets
    QFile flw(":/syntax/flutter_widgets");
    flw.open(QIODevice::ReadOnly);
    QTextStream flwin(&flw);
    while (!flwin.atEnd()) {
        k = flwin.readLine();
        if (k == "") continue;
        flutterObjectsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addJSExtDartObject(k);
    }
    flw.close();

    // flutter functions
    QFile flf(":/syntax/flutter_functions");
    flf.open(QIODevice::ReadOnly);
    QTextStream flfin(&flf);
    while (!flfin.atEnd()) {
        k = flfin.readLine();
        if (k == "") continue;
        flutterFunctionsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addJSExtDartFunction(k);
    }
    flf.close();
}

void CompleteWords::loadPHPWords()
{
    QString k;

    // php functions
    QFile ff(":/syntax/php_functions");
    ff.open(QIODevice::ReadOnly);
    QTextStream fin(&ff);
    while (!fin.atEnd()) {
        k = fin.readLine();
        if (k == "") continue;
        QString kName = "", kParams = "";
        int kSep = k.indexOf("(");
        if (kSep > 0) {
            kName = k.mid(0, kSep).trimmed();
            kParams = k.mid(kSep).trimmed();
            phpFunctionsComplete[kName.toStdString()] = kParams.toStdString();
            tooltipsPHP[kName.toStdString()] = kParams.replace("<", "&lt;").replace(">", "&gt;").toStdString();
            HighlightWords::addPHPFunction(kName);
        } else {
            phpFunctionsComplete[k.toStdString()] = k.toStdString();
            HighlightWords::addPHPFunction(k);
        }
    }
    ff.close();

    // php consts
    QFile cnf(":/syntax/php_consts");
    cnf.open(QIODevice::ReadOnly);
    QTextStream cnin(&cnf);
    while (!cnin.atEnd()) {
        k = cnin.readLine();
        if (k == "") continue;
        phpConstsComplete[k.toStdString()] = k.toStdString();
    }
    cnf.close();

    // php classes
    QFile cf(":/syntax/php_classes");
    cf.open(QIODevice::ReadOnly);
    QTextStream cin(&cf);
    while (!cin.atEnd()) {
        k = cin.readLine();
        if (k == "") continue;
        QString kName = "", kParams = "";
        int kSep = k.indexOf("(");
        if (kSep > 0) {
            kName = k.mid(0, kSep).trimmed();
            kParams = k.mid(kSep).trimmed();
            phpClassesComplete[kName.toStdString()] = kParams.toStdString();
            tooltipsPHP[kName.toStdString()] = kParams.replace("<", "&lt;").replace(">", "&gt;").toStdString();
            QStringList classParts = kName.split("\\");
            for (int i=0; i<classParts.size(); i++) {
                QString classPart = classParts.at(i);
                if (classPart.size() == 0) continue;
                HighlightWords::addPHPClass(classPart);
            }
        } else {
            phpClassesComplete[k.toStdString()] = k.toStdString();
            QStringList classParts = k.split("\\");
            for (int i=0; i<classParts.size(); i++) {
                QString classPart = classParts.at(i);
                if (classPart.size() == 0) continue;
                HighlightWords::addPHPClass(classPart);
            }
        }
    }
    cf.close();

    // php class methods
    QFile mf(":/syntax/php_class_methods");
    mf.open(QIODevice::ReadOnly);
    QTextStream min(&mf);
    while (!min.atEnd()) {
        k = min.readLine();
        if (k == "") continue;
        QString kName = "", kParams = "";
        int kSep = k.indexOf("(");
        if (kSep > 0) {
            kName = k.mid(0, kSep).trimmed();
            kParams = k.mid(kSep).trimmed();
            phpClassMethodsComplete[kName.toStdString()] = kParams.toStdString();
            tooltipsPHP[kName.toStdString()] = kParams.replace("<", "&lt;").replace(">", "&gt;").toStdString();
            //HighlightWords::addPHPFunction(kName);
        } else {
            phpClassMethodsComplete[k.toStdString()] = k.toStdString();
            //HighlightWords::addPHPFunction(k);
        }
    }
    mf.close();

    // php class consts
    QFile cof(":/syntax/php_class_consts");
    cof.open(QIODevice::ReadOnly);
    QTextStream coin(&cof);
    while (!coin.atEnd()) {
        k = coin.readLine();
        if (k == "") continue;
        phpClassConstsComplete[k.toStdString()] = k.toStdString();
        QStringList kParts = k.split("::");
        if (kParts.size() == 2) HighlightWords::addPHPClassConstant(kParts.at(0), kParts.at(1));
    }
    cof.close();

    // php class props
    QFile pof(":/syntax/php_class_props");
    pof.open(QIODevice::ReadOnly);
    QTextStream poin(&pof);
    while (!poin.atEnd()) {
        k = poin.readLine();
        if (k == "") continue;
        phpClassPropsComplete[k.toStdString()] = k.toStdString();
    }
    pof.close();

    // php globals
    QFile of(":/syntax/php_globals");
    of.open(QIODevice::ReadOnly);
    QTextStream oin(&of);
    while (!oin.atEnd()) {
        k = oin.readLine();
        if (k == "") continue;
        phpGlobalsComplete[k.toStdString()] = k.toStdString();
        HighlightWords::addPHPVariable(k);
    }
    of.close();

    // php specials
    QFile sf(":/syntax/php_specials");
    sf.open(QIODevice::ReadOnly);
    QTextStream sin(&sf);
    while (!sin.atEnd()) {
        k = sin.readLine();
        if (k == "") continue;
        phpSpecialsComplete[k.toStdString()] = k.toStdString();
    }
    sf.close();

    // php function types
    QFile ftf(":/syntax/php_function_types");
    ftf.open(QIODevice::ReadOnly);
    QTextStream ftin(&ftf);
    while (!ftin.atEnd()) {
        k = ftin.readLine();
        if (k == "") continue;
        QStringList kList = k.split(" ");
        if (kList.size() != 2) continue;
        phpFunctionTypes[kList.at(0).toStdString()] = kList.at(1).toStdString();
    }
    ftf.close();

    // php class method types
    QFile mtf(":/syntax/php_class_method_types");
    mtf.open(QIODevice::ReadOnly);
    QTextStream mtin(&mtf);
    while (!mtin.atEnd()) {
        k = mtin.readLine();
        if (k == "") continue;
        QStringList kList = k.split(" ");
        if (kList.size() != 2) continue;
        phpClassMethodTypes[kList.at(0).toStdString()] = kList.at(1).toStdString();
    }
    mtf.close();

    // php magic methods
    QFile magf(":/syntax/php_magic");
    magf.open(QIODevice::ReadOnly);
    QTextStream magin(&magf);
    while (!magin.atEnd()) {
        k = magin.readLine();
        if (k == "") continue;
        phpMagicComplete[k.toStdString()] = k.toStdString();
    }
    magf.close();
}
