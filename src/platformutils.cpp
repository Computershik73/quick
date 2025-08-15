#include "platformutils.h"

#include <QApplication>
#include "debug.h"

#if defined(Q_OS_WIN32) && QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
#include <QtWinExtras/QtWin>
#define DWM_FEATURES
#endif

#ifdef SYMBIAN3_READY
#include <akndiscreetpopup.h>
#endif

#ifdef Q_OS_SYMBIAN
#include <apgcli.h>
#include <apgtask.h>
#include <eikenv.h>
#endif
#include <QDesktopServices>

#include <crypto.h>

PlatformUtils::PlatformUtils(QObject *parent)
    : QObject(parent)
    , window(dynamic_cast<QWidget*>(parent))
    #if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    , trayIcon(this)
    , trayMenu()
    #endif
    , unread()
    #ifdef SYMBIAN3_READY
    , pigler()
    , piglerId(-1)
    #endif
{
    if (window) {
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->setAttribute(Qt::WA_QuitOnClose, false);
    }

#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    connect(&trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(&trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
    connect(&trayMenu, SIGNAL(triggered(QAction*)), this, SLOT(menuTriggered(QAction*)));

    trayMenu.addAction("Open Kutegram");
    trayMenu.addAction("Exit");

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.setIcon(QIcon(":/kutegramquick_small.png"));
    trayIcon.setToolTip("Kutegram");
    trayIcon.show();
#endif

#ifdef SYMBIAN3_READY
    qint32 response = pigler.init("Kutegram"); //TODO think about randomization
    if (response >= 0) {
        if (response > 0)
            piglerHandleTap(response);

        connect(&pigler, SIGNAL(handleTap(qint32)), this, SLOT(piglerHandleTap(qint32)));
        pigler.removeAllNotifications();
        piglerId = 0;
    }
#endif
}

#ifdef SYMBIAN3_READY
void PlatformUtils::piglerHandleTap(qint32 notificationId)
{
    //App should be opened automatically
    unread.clear();
}
#endif

void PlatformUtils::showAndRaise()
{
    //TODO remove notifications
    unread.clear();

    if (window) {
        window->show();
        window->activateWindow();
        window->raise();
    }
}

void PlatformUtils::quit()
{
    QApplication::exit();
}

#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
void PlatformUtils::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason != QSystemTrayIcon::Context) {
        showAndRaise();
    }
}

void PlatformUtils::messageClicked()
{
    showAndRaise();
}

void PlatformUtils::menuTriggered(QAction *action)
{
    if (action->text() == "Exit") {
        quit();
        return;
    }

    showAndRaise();
}
#endif

void PlatformUtils::windowsExtendFrameIntoClientArea(int left, int top, int right, int bottom)
{
#ifdef DWM_FEATURES
    if (window) {
        window->setAttribute(Qt::WA_TranslucentBackground, true);
        window->setAttribute(Qt::WA_NoSystemBackground, false);
        window->setStyleSheet("background: transparent");
        QtWin::extendFrameIntoClientArea(window, left, top, right, bottom);
    }
#endif
}

bool PlatformUtils::windowsIsCompositionEnabled()
{
#ifdef DWM_FEATURES
    return QtWin::isCompositionEnabled();
#else
    return false;
#endif
}

QColor PlatformUtils::windowsRealColorizationColor()
{
#ifdef DWM_FEATURES
    return QtWin::realColorizationColor();
#else
    return Qt::white;
#endif
}

bool PlatformUtils::isWindows()
{
#ifdef DWM_FEATURES
    return true;
#else
    return false;
#endif
}

void PlatformUtils::gotNewMessage(qint64 peerId, QString peerName, QString senderName, QString text, bool silent)
{
    if (window && window->hasFocus()) {
        unread.clear();
        return;
    }

    QVariantMap info;
    info["id"] = peerId;
    info["peerName"] = peerName;
    info["senderName"] = senderName;
    info["text"] = text;

    unread.insert(peerId, info);

    QString title;
    QString message;

    title = peerName;
    message = senderName;
    message += text;

#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    if (!silent) {
        kgDebug() << "Sending Windows notification";
        trayIcon.showMessage(title, message);
    }
#endif

    if (unread.size() != 1) {
        title = "New messages from " + QString::number(unread.size()) + " chats";
        foreach (qint32 pid, unread.keys()) {
            if (!message.isEmpty()) {
                message += ", ";
            }
            message += unread[pid]["peerName"].toString();
        }
    }

    title = title.left(63);
    message = message.left(63);

#ifdef SYMBIAN3_READY
    kgDebug() << "Sending Symbian notification";
    TUid symbianUid = {SYMBIAN_UID};
    //TODO: icon
    TRAP_IGNORE(CAknDiscreetPopup::ShowGlobalPopupL(TPtrC16(title.utf16()), TPtrC16(message.utf16()), KAknsIIDNone, KNullDesC, 0, 0, 1, 0, 0, symbianUid));
#endif

#ifdef SYMBIAN3_READY
    kgDebug() << "Sending Pigler notification";
    if (piglerId == 0) {
        piglerId = pigler.createNotification(title, message);
    } else if (piglerId > 0) {
        pigler.updateNotification(piglerId, title, message);
    } else {
        kgDebug() << "Pigler is not initialized";
    }

    if (piglerId > 0) {
        static QImage piglerImage(":/kutegramquick_pigler.png");
        pigler.setNotificationIcon(piglerId, piglerImage);
    }
#endif

    //TODO: notify only when unfocused?
    //TODO: custom notification popup for Windows/legacy Symbian
    //TODO: android
    //TODO: vibrate
    //TODO: sound
    //TODO: blink
}

void openUrl(QUrl url)
{
#ifdef Q_OS_SYMBIAN

    // 1. Определяем, какой URL будем открывать
    QString mimeType = "application/x-web-browse"; // MIME-тип для обычного браузера
    bool useXPreviewer = false;
    const TUid KUidXPreviewer = { 0xE11B29D3 };
    QRegExp rx("^(https?://)?(twitter\\.com|x\\.com)/[A-Za-z0-9_]{1,15}/status/\\d+(/.*)?$", Qt::CaseInsensitive);
    if (rx.exactMatch(url.toString())) {
        // Если это ссылка на твит
        mimeType = "x-scheme-handler/xpreview"; // используем наш MIME-тип
        useXPreviewer = true;
    }

    qDebug() << "Открываю URL:" << url.toString() << "с MIME-типом:" << mimeType;

    // Вызываем Symbian API с правильными данными
    TRAPD(err, {

          // Получаем доступ к сессии оконного сервера
          RWsSession& wsSession = CCoeEnv::Static()->WsSession();
            TApaTaskList taskList(wsSession);
    TApaTask task = taskList.FindApp(KUidXPreviewer);

    if (task.Exists()) {
        qDebug() << "XPreviewer is already running. Terminating it...";
        // Если приложение найдено, просим его завершиться
        task.EndTask();

        // Даем системе небольшую паузу (например, 400 мс),
        // чтобы процесс успел завершиться.
        User::After(400000);
    } else {
        qDebug() << "XPreviewer is not running. Proceeding with launch.";
    }



    QString encUrl = QString::fromUtf8(url.toEncoded());
    TPtrC tUrl(TPtrC16(static_cast<const TUint16*>(encUrl.utf16()), encUrl.length()));

    RApaLsSession appArcSession;
    User::LeaveIfError(appArcSession.Connect());
    CleanupClosePushL<RApaLsSession>(appArcSession);

    // Ищем UID приложения по MIME-типу
    TDataType mimeDatatype(TPtrC8((const TUint8*)mimeType.toAscii().constData()));
    TUid handlerUID;
    appArcSession.AppForDataType(mimeDatatype, handlerUID);

    // Если для нашей схемы ничего не найдено (или для веб), используем браузер по умолчанию
    if (handlerUID.iUid == 0 || handlerUID.iUid == -1) {
        static TUid KUidBrowser = {0x10008D39};
        handlerUID = KUidBrowser;
    }

    // Формируем буфер С АРГУМЕНТОМ (URL).
    // УБИРАЕМ ПРЕФИКС "4 ", если только мы не вызываем стандартный браузер принудительно.
    HBufC* argument = HBufC::NewLC(tUrl.Length() + (useXPreviewer ? 0 : 4));
    TPtr argumentPtr = argument->Des();

    if (!useXPreviewer) {
        _LIT(KBrowserPrefix, "4 ");
        argumentPtr.Copy(KBrowserPrefix); // Добавляем префикс только для браузера
    }
    argumentPtr.Append(tUrl);


    TApaTaskList taskList2(CCoeEnv::Static()->WsSession());
    TApaTask task2 = taskList2.FindApp(handlerUID);

    if (task2.Exists()) {
        task2.BringToForeground();
        HBufC8* param8 = HBufC8::NewLC(argument->Length());
        param8->Des().Append(*argument);
        task2.SendMessage(TUid::Uid(0), *param8);
        CleanupStack::PopAndDestroy(param8);
    } else {
        TThreadId id;
        appArcSession.StartDocument(*argument, handlerUID, id);
        //}

        CleanupStack::PopAndDestroy(argument);
        CleanupStack::PopAndDestroy(&appArcSession);
    }
    });

if (err != KErrNone) {
    qDebug() << "Symbian API error:" << err;
}

#else
    QDesktopServices::openUrl(url);
#endif
}
