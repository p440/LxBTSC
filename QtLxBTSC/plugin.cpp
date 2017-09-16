﻿/*
 * TeamSpeak 3 demo plugin
 *
 * Copyright (c) 2008-2017 TeamSpeak Systems GmbH
 */

#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "teamspeak/clientlib_publicdefinitions.h"
#include "ts3_functions.h"
#include "plugin.h"
#include <QtGuiClass.h>
#include <QApplication>
#include <QMainWindow>
#include <QtGuiClass.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMessageBox>
#include <QtWidgets/QVBoxLayout>
//#include <QStackedWidget>
#include <QFile>
#include <QRegularExpression>
#include <QTimer>
#include "bbcode_parser.h"


static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 22

#define PATH_BUFSIZE 512
//#define COMMAND_BUFSIZE 128
//#define INFODATA_BUFSIZE 128
//#define SERVERINFO_BUFSIZE 256
//#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if(WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if(!result) {
		const wchar_t* name = L"Better Chat";
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = "Better Chat";  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return "Better Chat";
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
    return "1.0";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Luch";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Better text chat";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
uint64 currentServerID;
QtGuiClass *chat;
QMap<uint64, QMap<anyID, QString> > clients;
QJsonObject emotes;
QTabWidget *chatTabWidget;
QMetaObject::Connection c;
QMetaObject::Connection d;
QString pathToPlugin;
QTimer *timer;
bbcode::parser parser;
bool first = true;

// read the emotes file
void readEmoteJson(QString path)
{
	QString text;
	QFile f;
	QString fullPath = path.append("LxBTSC\\template\\emotes.json");
	f.setFileName(fullPath);
	f.open(QIODevice::ReadOnly | QIODevice::Text);
	text = f.readAll();
	f.close();
	QJsonDocument dox = QJsonDocument::fromJson(text.toUtf8());
	emotes = dox.object();
}

// some info
static void receive(int i)
{
	//QMessageBox::information(0, "debug", "tabchange_trigger", QMessageBox::Ok);
	if (i >= 0)
	{
		QString tabName;
		if (i == 0)
		{
			tabName = QString("tab-%1-server").arg(currentServerID);
		}
		else if (i == 1)
		{
			tabName = QString("tab-%1-channel").arg(currentServerID);
		}
		else
		{
			tabName = QString("tab-%1-%2").arg(currentServerID).arg(chatTabWidget->tabText(i));
		}
		chat->switchTab(tabName);
	}
	//QMessageBox::information(0, "debug", "tabchange_done", QMessageBox::Ok);
}

static void recheck()
{
	if (currentServerID != NULL)
	{
		//QMessageBox::information(0, "debug", QString("recheck_trigger: %1").arg(currentServerID), QMessageBox::Ok);
		int i = chatTabWidget->currentIndex();
		if (i >= 0)
		{
			QString tabName;
			if (i == 0)
			{
				tabName = QString("tab-%1-server").arg(currentServerID);
			}
			else if (i == 1)
			{
				tabName = QString("tab-%1-channel").arg(currentServerID);
			}
			else
			{
				tabName = QString("tab-%1-%2").arg(currentServerID).arg(chatTabWidget->tabText(i));
			}
			chat->switchTab(tabName);
		}
		//QMessageBox::information(0, "debug", "recheck_done", QMessageBox::Ok);
	}
}

static void tabCloseReceive(int i)
{
	if (i > 1)
	{
		QString tabName = QString("tab-%1-server").arg(currentServerID);
		chat->switchTab(tabName);
		chatTabWidget->setCurrentIndex(0);
	}
}

// find the widget containing chat tabs and store it for later use
void findChatTabWidget()
{
	QWidgetList list = qApp->allWidgets();
	for (int i = 0; i < list.count(); i++)
	{
		if (list[i]->objectName() == "ChatTabWidget")
		{
			chatTabWidget = static_cast<QTabWidget*>(list[i]);
			QWidget *parent = chatTabWidget->parentWidget();
			
			//QMessageBox::information(0, "debug", "widget_add", QMessageBox::Ok);
			static_cast<QBoxLayout*>(parent->layout())->insertWidget(0, chat);
			//QMessageBox::information(0, "debug", "widget_add_done", QMessageBox::Ok);

			chatTabWidget->setMinimumHeight(24);
			chatTabWidget->setMaximumHeight(24);

			c = QObject::connect(chatTabWidget, &QTabWidget::currentChanged, receive);
			//c = QObject::connect(chatTabWidget, &QTabWidget::tabBarClicked, receive);
			d = QObject::connect(chatTabWidget, &QTabWidget::tabCloseRequested, tabCloseReceive);
			chatTabWidget->setMovable(false);
			
			break;
		}
	}
}

void disconnectChatWidget()
{
	// disconnect or crash
	QObject::disconnect(c);
	QObject::disconnect(d);
}

// init plugin
int ts3plugin_init() {
	char pluginPath[PATH_BUFSIZE];
	
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);
	pathToPlugin = QString(pluginPath);
	readEmoteJson(pathToPlugin);

	timer = new QTimer();
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, recheck);

	//QMessageBox::information(0, "debug", "init", QMessageBox::Ok);
	chat = new QtGuiClass(pathToPlugin);
	chat->setStyleSheet("border: 1px solid gray");
	//QMessageBox::information(0, "debug", "init_done", QMessageBox::Ok);

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
	/* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
	disconnectChatWidget();
	delete timer;
	delete chat;
	//delete lWidget;
	//delete chatTabWidget;
	
	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
//int ts3plugin_offersConfigure() {
//	/*
//	 * Return values:
//	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
//	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
//	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
//	 */
//	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
//}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
//void ts3plugin_configure(void* handle, void* qParentWidget) {
//}

/*
 * If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
 * automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "";
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
	currentServerID = serverConnectionHandlerID;
	if (first == false)
	{
		timer->stop();
		timer->start(500);
	}
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 0;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */
QMap<unsigned short, QString> getAllClientNicks(uint64 serverConnectionHandlerID)
{
	QMap<unsigned short, QString> map;
	anyID *list;
	ts3Functions.getClientList(serverConnectionHandlerID, &list);
	for(size_t i = 0; list[i]; i++)
	{
		char res[TS3_MAX_SIZE_CLIENT_NICKNAME];
		ts3Functions.getClientDisplayName(serverConnectionHandlerID, list[i], res, TS3_MAX_SIZE_CLIENT_NICKNAME);
		map.insert(list[i], QString(res));
	}
	return map;
}

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
    /* Some example code following to show how to use the information query functions. */
	
	if (newStatus == STATUS_CONNECTION_ESTABLISHED)
	{
		if (first)
		{
			//QMessageBox::information(0, "debug", "first_connect", QMessageBox::Ok);
			// Add new chat widget to the UI       ??does serverConnectionHandlerID stay same during teamspeak session even if disconnect/connect several times??
			findChatTabWidget();
			first = false;
			//QMessageBox::information(0, "debug", "first_connect_done", QMessageBox::Ok);
		}
		//QMessageBox::information(0, "debug", "add_server", QMessageBox::Ok);
		chat->addServer(serverConnectionHandlerID);
		//QMessageBox::information(0, "debug", "add_server_done", QMessageBox::Ok);
		clients.insert(serverConnectionHandlerID, getAllClientNicks(serverConnectionHandlerID));
		chat->messageReceived2(QString("<img class=\"incoming\"><span><%1> <span class=\"good\">Server Connected</span></span>").arg(QTime::currentTime().toString("hh:mm:ss")), QString("tab-%1-server").arg(serverConnectionHandlerID));
	}
	if (newStatus == STATUS_DISCONNECTED)
	{
		chat->messageReceived2(QString("<img class=\"incoming\"><span><%1> <span class=\"bad\">Server Disconnected</span></span>").arg(QTime::currentTime().toString("hh:mm:ss")), QString("tab-%1-server").arg(serverConnectionHandlerID));
		clients.remove(serverConnectionHandlerID);
	}
}

//void ts3plugin_onNewChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID) {
//}

//void ts3plugin_onNewChannelCreatedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
//}

//void ts3plugin_onDelChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
//}

//void ts3plugin_onChannelMoveEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 newChannelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
//}

//void ts3plugin_onUpdateChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
//}

//void ts3plugin_onUpdateChannelEditedEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
//}

//void ts3plugin_onUpdateClientEvent(uint64 serverConnectionHandlerID, anyID clientID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
//}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
	if (oldChannelID == 0)
	{
		//client connected
		char res[TS3_MAX_SIZE_CLIENT_NICKNAME];
		ts3Functions.getClientDisplayName(serverConnectionHandlerID, clientID, res, TS3_MAX_SIZE_CLIENT_NICKNAME);
		clients[serverConnectionHandlerID].insert(clientID, QString(res));

		//servers.value(serverConnectionHandlerID)->messageReceived2(QString("<img class=\"incoming\"><span><%1> <span class=\"good\">%2 Joined</span></span>").arg(QTime::currentTime().toString("hh:mm:ss"), QString(res)), "0");
		chat->messageReceived2(QString("<img class=\"incoming\"><span><%1> <span class=\"good\">%2 Joined</span></span>").arg(QTime::currentTime().toString("hh:mm:ss"), QString(res)), QString("tab-%1-server").arg(serverConnectionHandlerID));
	}
	if (newChannelID == 0)
	{
		//client disconnected
		//char res[TS3_MAX_SIZE_CLIENT_NICKNAME];
		//ts3Functions.getClientDisplayName(serverConnectionHandlerID, clientID, res, TS3_MAX_SIZE_CLIENT_NICKNAME);
		QString name = clients[serverConnectionHandlerID].take(clientID);
		chat->messageReceived2(QString("<img class=\"incoming\"><span><%1> <span class=\"bad\">%2 Left</span></span>").arg(QTime::currentTime().toString("hh:mm:ss"), name), QString("tab-%1-server").arg(serverConnectionHandlerID));
	}
}

//void ts3plugin_onClientMoveSubscriptionEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility) {
//}

//void ts3plugin_onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage) {
//}

//void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage) {
//}

//void ts3plugin_onClientKickFromChannelEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
//}

//void ts3plugin_onClientKickFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
//}

void ts3plugin_onClientIDsEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, anyID clientID, const char* clientName) {
}

void ts3plugin_onClientIDsFinishedEvent(uint64 serverConnectionHandlerID) {
}

//void ts3plugin_onServerEditedEvent(uint64 serverConnectionHandlerID, anyID editerID, const char* editerName, const char* editerUniqueIdentifier) {
//}

//void ts3plugin_onServerUpdatedEvent(uint64 serverConnectionHandlerID) {
//}

int ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage) {
	if(returnCode) {
		/* A plugin could now check the returnCode with previously (when calling a function) remembered returnCodes and react accordingly */
		/* In case of using a a plugin return code, the plugin can return:
		 * 0: Client will continue handling this error (print to chat tab)
		 * 1: Client will ignore this error, the plugin announces it has handled it */
		return 1;
	}
	return 0;  /* If no plugin return code was used, the return value of this function is ignored */
}

//void ts3plugin_onServerStopEvent(uint64 serverConnectionHandlerID, const char* shutdownMessage) {
//}

QString isAnimated(bool animated)
{
	if (animated)
	{
		return "gif";
	}
	return "png";
}

QString urltags(QString original)
{
	// replace url bbcode tags
	//QMessageBox::information(0, "url", original, QMessageBox::Ok);
	original = original.toHtmlEscaped();
	QRegularExpression url(QString("\\[URL(=(.*?))?\\](.*?)\\[\\/URL\\]"));
	QRegularExpressionMatchIterator iterator = url.globalMatch(original);
	while (iterator.hasNext())
	{
		QRegularExpressionMatch match = iterator.next();
		//QMessageBox::information(0, "url", match.captured(0), QMessageBox::Ok);
		QString htmlurl;
		if (!match.captured(2).isNull())
		{
			htmlurl = QString("<a href=\"%1\">%2</a>").arg(match.captured(2), match.captured(3));
		}
		else
		{
			htmlurl = QString("<a href=\"%1\">%2</a>").arg(match.captured(3), match.captured(3));
		}
		
		original.replace(match.captured(0), htmlurl);
	}
	return original;
}

// replace emote text with html <img>
QString emoticonize(QString original)
{
	// newlines to br
	original.replace(QRegExp("[\r\n]"), "</br>");
	
	// escape single quotes
	original.replace("'", "\\'");
	// add embedded youtube video
	QRegExp yt("http(?:s?):\\/\\/(?:www\\.)?youtu(?:be\\.com\\/watch\\?v=|\\.be\\/)([\\w\\-\\_]*)(&(amp;)?[\\w\\?=]*)?");
	int i = yt.indexIn(original);
	if (i > 0)
	{
		QStringList list = yt.capturedTexts();
		original.append(QString("</br><iframe frameborder=\"0\" src=\"https://www.youtube.com/embed/%1\" ></iframe>").arg(list.value(1)));
	}
	// replace emoticons
	QStringList keys = emotes.keys();
	foreach(QString value, keys)
	{
		QRegularExpression rx(QString("(?!<a[^>]*?>)(%1)(?![^<]*?</a>)").arg(value));
		
		//original.replace(rx, QString("<img class=\"%1\" />").arg(emotes[value].toString()));
		original.replace(rx, QString("<img class=\"%1\" src=\"Emotes/%1.%2\" title=\"%1\" />").arg(emotes[value].toObject()["name"].toString(), isAnimated(emotes[value].toObject()["animated"].toBool())));
	}
	return original;
}

// was message received or sent
QString direction(bool outgoing)
{
	if (outgoing)
	{
		return "outgoing";
	}
	return "incoming";
}

// parse bbcode, emoticonize
QString format(QString message, const char* name, bool outgoing)
{
	QTime t = QTime::currentTime();
	stringstream str;
	//str << message;
	str << urltags(message).toStdString();
	parser.source_stream(str);
	parser.parse();
	return QString("<img class=\"%1\"><span><%2> <span class=\"name\">\"%3\"</span>: %4</span>").arg(direction(outgoing), t.toString("hh:mm:ss"), QString(name), emoticonize(QString::fromStdString(parser.content())));
}

// ts3 client received a text message
int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {

	/* Friend/Foe manager has ignored the message, so ignore here as well. */
	if(ffIgnored) {
		return 0; /* Client will ignore the message anyways, so return value here doesn't matter */
	}

	// get clients own ID
	anyID myID;
	if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
		ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
		return 0;
	}
	bool outgoing = false;
	if (myID == fromID) {
		outgoing = true;
	}
	//// ??do clientid stay same across session even if disconnect/reconnect multiple times??
	QString key;
	if (targetMode == 3)
	{
		key = QString("tab-%1-server").arg(serverConnectionHandlerID);
	}
	else if (targetMode == 2)
	{
		key = QString("tab-%1-channel").arg(serverConnectionHandlerID);
	}
	else
	{
		if (fromID == myID)
		{
			char res[TS3_MAX_SIZE_CLIENT_NICKNAME];
			ts3Functions.getClientDisplayName(serverConnectionHandlerID, toID, res, TS3_MAX_SIZE_CLIENT_NICKNAME);
			//key = res;
			key = QString("tab-%1-%2").arg(serverConnectionHandlerID).arg(res);
		}
		else
		{
			key = fromName;
		}
	}
	QString m(message);
	if (m.startsWith("!embed "))
	{
		QStringList l = m.split(' ');
		if (l.count() > 1)
		{
			l[1].remove(QRegularExpression("\\[\\/?URL\\]"));
			chat->messageReceived2(QString("<img class=\"%1 embedded-image\"><span><%2> <span class=\"name\">\"%3\"</span>:<a href=\"%4\"><img src=\"%4\"/></a></span>").arg(direction(outgoing), QTime::currentTime().toString("hh:mm:ss"), QString(fromName), l.value(1)), key);
			return 0;
		}
	}
	
	chat->messageReceived2(format(message, fromName, outgoing), key);
    return 0;  /* 0 = handle normally, 1 = client will ignore the text message */
}

//void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
//}

//void ts3plugin_onConnectionInfoEvent(uint64 serverConnectionHandlerID, anyID clientID) {
//}

//void ts3plugin_onServerConnectionInfoEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onChannelSubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
//}

//void ts3plugin_onChannelSubscribeFinishedEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onChannelUnsubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
//}

//void ts3plugin_onChannelUnsubscribeFinishedEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onChannelDescriptionUpdateEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
//}

//void ts3plugin_onChannelPasswordChangedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
//}

//void ts3plugin_onPlaybackShutdownCompleteEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onSoundDeviceListChangedEvent(const char* modeID, int playOrCap) {
//}

//void ts3plugin_onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels) {
//}

//void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {
//}

//void ts3plugin_onEditMixedPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {
//}

//void ts3plugin_onEditCapturedVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited) {
//}

//void ts3plugin_onCustom3dRolloffCalculationClientEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance, float* volume) {
//}

//void ts3plugin_onCustom3dRolloffCalculationWaveEvent(uint64 serverConnectionHandlerID, uint64 waveHandle, float distance, float* volume) {
//}

//void ts3plugin_onUserLoggingMessageEvent(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString) {
//}

/* Clientlib rare */

//void ts3plugin_onClientBanFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, uint64 time, const char* kickMessage) {
//}

//int ts3plugin_onClientPokeEvent(uint64 serverConnectionHandlerID, anyID fromClientID, const char* pokerName, const char* pokerUniqueIdentity, const char* message, int ffIgnored) {
//    return 0;  /* 0 = handle normally, 1 = client will ignore the poke */
//}

//void ts3plugin_onClientSelfVariableUpdateEvent(uint64 serverConnectionHandlerID, int flag, const char* oldValue, const char* newValue) {
//}

//void ts3plugin_onFileListEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path, const char* name, uint64 size, uint64 datetime, int type, uint64 incompletesize, const char* returnCode) {
//}

//void ts3plugin_onFileListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path) {
//}

//void ts3plugin_onFileInfoEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* name, uint64 size, uint64 datetime) {
//}

//void ts3plugin_onServerGroupListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, const char* name, int type, int iconID, int saveDB) {
//}

//void ts3plugin_onServerGroupListFinishedEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onServerGroupByClientIDEvent(uint64 serverConnectionHandlerID, const char* name, uint64 serverGroupList, uint64 clientDatabaseID) {
//}

//void ts3plugin_onServerGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
//}

//void ts3plugin_onServerGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID) {
//}

//void ts3plugin_onServerGroupClientListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, uint64 clientDatabaseID, const char* clientNameIdentifier, const char* clientUniqueID) {
//}

//void ts3plugin_onChannelGroupListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, const char* name, int type, int iconID, int saveDB) {
//}

//void ts3plugin_onChannelGroupListFinishedEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onChannelGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
//}

//void ts3plugin_onChannelGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID) {
//}

//void ts3plugin_onChannelPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
//}

//void ts3plugin_onChannelPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
//}

//void ts3plugin_onClientPermListEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
//}

//void ts3plugin_onClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID) {
//}

//void ts3plugin_onChannelClientPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
//}

//void ts3plugin_onChannelClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID) {
//}

//void ts3plugin_onClientChannelGroupChangedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, uint64 channelID, anyID clientID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
//}

//int ts3plugin_onServerPermissionErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, unsigned int failedPermissionID) {
//	return 0;  /* See onServerErrorEvent for return code description */
//}

//void ts3plugin_onPermissionListGroupEndIDEvent(uint64 serverConnectionHandlerID, unsigned int groupEndID) {
//}

//void ts3plugin_onPermissionListEvent(uint64 serverConnectionHandlerID, unsigned int permissionID, const char* permissionName, const char* permissionDescription) {
//}

//void ts3plugin_onPermissionListFinishedEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onPermissionOverviewEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, uint64 channelID, int overviewType, uint64 overviewID1, uint64 overviewID2, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
//}

//void ts3plugin_onPermissionOverviewFinishedEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onServerGroupClientAddedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
//}

//void ts3plugin_onServerGroupClientDeletedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
//}

//void ts3plugin_onClientNeededPermissionsEvent(uint64 serverConnectionHandlerID, unsigned int permissionID, int permissionValue) {
//}

//void ts3plugin_onClientNeededPermissionsFinishedEvent(uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onFileTransferStatusEvent(anyID transferID, unsigned int status, const char* statusMessage, uint64 remotefileSize, uint64 serverConnectionHandlerID) {
//}

//void ts3plugin_onClientChatClosedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientUniqueIdentity) {
//}

//void ts3plugin_onClientChatComposingEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientUniqueIdentity) {
//}

//void ts3plugin_onServerLogEvent(uint64 serverConnectionHandlerID, const char* logMsg) {
//}

//void ts3plugin_onServerLogFinishedEvent(uint64 serverConnectionHandlerID, uint64 lastPos, uint64 fileSize) {
//}

//void ts3plugin_onMessageListEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, uint64 timestamp, int flagRead) {
//}

//void ts3plugin_onMessageGetEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, const char* message, uint64 timestamp) {
//}

//void ts3plugin_onClientDBIDfromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID) {
//}

//void ts3plugin_onClientNamefromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {
//}

//void ts3plugin_onClientNamefromDBIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {
//}

//void ts3plugin_onComplainListEvent(uint64 serverConnectionHandlerID, uint64 targetClientDatabaseID, const char* targetClientNickName, uint64 fromClientDatabaseID, const char* fromClientNickName, const char* complainReason, uint64 timestamp) {
//}

//void ts3plugin_onBanListEvent(uint64 serverConnectionHandlerID, uint64 banid, const char* ip, const char* name, const char* uid, uint64 creationTime, uint64 durationTime, const char* invokerName,
//							  uint64 invokercldbid, const char* invokeruid, const char* reason, int numberOfEnforcements, const char* lastNickName) {
//}

//void ts3plugin_onClientServerQueryLoginPasswordEvent(uint64 serverConnectionHandlerID, const char* loginPassword) {
//}

//void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID, const char* pluginName, const char* pluginCommand) {
//}

//void ts3plugin_onIncomingClientQueryEvent(uint64 serverConnectionHandlerID, const char* commandText) {
//}

//void ts3plugin_onServerTemporaryPasswordListEvent(uint64 serverConnectionHandlerID, const char* clientNickname, const char* uniqueClientIdentifier, const char* description, const char* password, uint64 timestampStart, uint64 timestampEnd, uint64 targetChannelID, const char* targetChannelPW) {
//}

/* Client UI callbacks */

/*
 * Called from client when an avatar image has been downloaded to or deleted from cache.
 * This callback can be called spontaneously or in response to ts3Functions.getAvatar()
 */
//void ts3plugin_onAvatarUpdated(uint64 serverConnectionHandlerID, anyID clientID, const char* avatarPath) {
//}

/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 *
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
//void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
//}

/* This function is called if a plugin hotkey was pressed. Omit if hotkeys are unused. */
//void ts3plugin_onHotkeyEvent(const char* keyword) {
//}

/* Called when recording a hotkey has finished after calling ts3Functions.requestHotkeyInputDialog */
//void ts3plugin_onHotkeyRecordedEvent(const char* keyword, const char* key) {
//}

// This function receives your key Identifier you send to notifyKeyEvent and should return
// the friendly device name of the device this hotkey originates from. Used for display in UI.
//const char* ts3plugin_keyDeviceName(const char* keyIdentifier) {
//	return NULL;
//}

// This function translates the given key identifier to a friendly key name for display in the UI
//const char* ts3plugin_displayKeyText(const char* keyIdentifier) {
//	return NULL;
//}

// This is used internally as a prefix for hotkeys so we can store them without collisions.
// Should be unique across plugins.
//const char* ts3plugin_keyPrefix() {
//	return NULL;
//}

/* Called when client custom nickname changed */
void ts3plugin_onClientDisplayNameChanged(uint64 serverConnectionHandlerID, anyID clientID, const char* displayName, const char* uniqueClientIdentifier) {
	//servers->value(serverConnectionHandlerID)->nicknameChanged(QString(displa))
	clients[serverConnectionHandlerID].insert(clientID, QString(displayName));
}
