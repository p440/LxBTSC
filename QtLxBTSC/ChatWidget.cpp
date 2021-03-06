/*
 * Better Chat plugin for TeamSpeak 3
 * GPLv3 license
 *
 * Copyright (C) 2019 Luch (https://github.com/Luch00)
*/

#include "ChatWidget.h"
#include "FileTransferListWidget.h"
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QTimer>

ChatWidget::ChatWidget(const QString& path, TsWebObject* webObject, QWidget *parent)
    : QFrame(parent)
	, wObject(webObject)
	, pathToPage(QString("file:///%1LxBTSC/template/chat.html").arg(path))
	, view(new QWebEngineView(this))
	, verticalLayout(new QVBoxLayout(this))
	, menu(new QMenu(view))
	, copyAction(new QAction("Copy", this))
	, copyUrlAction(new QAction("Copy Link", this))
	, page(new TsWebEnginePage(view))
	, channel(new QWebChannel(page))
	, loadComplete(false)
{
	this->setObjectName(QStringLiteral("ChatWidget"));
		
	this->setStyleSheet("border: 1px solid gray");

	verticalLayout->setSpacing(1);
	verticalLayout->setContentsMargins(1, 1, 1, 1);
	verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	verticalLayout->addWidget(view);

	view->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

	connect(copyAction, &QAction::triggered, this, &ChatWidget::onCopyActivated);
	connect(copyUrlAction, &QAction::triggered, this, &ChatWidget::onCopyUrlActivated);
	connect(view, &QWebEngineView::customContextMenuRequested, this, &ChatWidget::onShowContextMenu);
	connect(view, &QWebEngineView::loadFinished, this, [=](bool ok)
	{
		if (!ok)
		{
			// don't trigger anything when loadFinished is triggered but load is not ok
			return;
		}
		if (!loadComplete)
		{
			loadComplete = true;
			logInfo("Page load finished");
		}
		else
		{
			emit pageReloaded();
			logInfo("Page reload finished");
		}
	});

	setupPage();
	view->setPage(page);
	waitloop();
}

ChatWidget::~ChatWidget()
{
}

void ChatWidget::waitloop() const
{
	if(!loadComplete)
	{
		QTimer timer;
		timer.setSingleShot(true);
		QEventLoop wait;
		connect(&timer, &QTimer::timeout, &wait, &QEventLoop::quit);
		logInfo("Waiting for page load...");
		timer.start(200);
		wait.exec();
		if (!loadComplete)
		{
			waitloop();
		}
	}
}

void ChatWidget::setupPage() const
{
	page->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
	page->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
	page->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
	page->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
	page->settings()->setUnknownUrlSchemePolicy(QWebEngineSettings::UnknownUrlSchemePolicy::AllowAllUnknownUrlSchemes);
	page->profile()->setHttpUserAgent(QString("Twitterbot/1.0 %1").arg(QWebEngineProfile::defaultProfile()->httpUserAgent()));

	connect(page, &TsWebEnginePage::fullScreenRequested, this, &ChatWidget::onFullScreenRequested);
	connect(page, &TsWebEnginePage::linkHovered, this, &ChatWidget::onLinkHovered);
	connect(page, &TsWebEnginePage::fileUrlClicked, this, &ChatWidget::fileUrlClicked);
	connect(page, &TsWebEnginePage::clientUrlClicked, this, &ChatWidget::clientUrlClicked);
	connect(page, &TsWebEnginePage::channelUrlClicked, this, &ChatWidget::channelUrlClicked);

	page->setWebChannel(channel);
	channel->registerObject("wObject", wObject);
	logInfo("Page load start");
	page->load(QUrl(pathToPage));
}

void ChatWidget::keyReleaseEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_C && QApplication::keyboardModifiers().testFlag(Qt::ControlModifier))
	{
		onCopyActivated();
		return;
	}
	QFrame::keyReleaseEvent(event);
}

void ChatWidget::onShowContextMenu(const QPoint &p)
{	
	menu->clear();
	if (page->hasSelection())
	{
		menu->addAction(copyAction);
	}
	if (!currentHoveredUrlTemp.isEmpty())
	{
		currentHoveredUrl = currentHoveredUrlTemp;
		menu->addAction(copyUrlAction);
	}
	if (!menu->actions().isEmpty())
	{
		menu->popup(view->mapToGlobal(p));
	}
}

void ChatWidget::onLinkHovered(const QUrl &u)
{
	currentHoveredUrlTemp = u;
	emit linkHovered(u);
}

void ChatWidget::onCopyActivated() const
{
	QGuiApplication::clipboard()->setText(page->selectedText(), QClipboard::Clipboard);
}

void ChatWidget::onCopyUrlActivated() const
{
	QGuiApplication::clipboard()->setText(currentHoveredUrl.toString(), QClipboard::Clipboard);
}

void ChatWidget::reload() const
{
	view->reload();
}

void ChatWidget::onFullScreenRequested(QWebEngineFullScreenRequest request)
{
	if (request.toggleOn())
	{
		if (fullScreenWindow)
			return;
		request.accept();
		fullScreenWindow.reset(new FullScreenWindow(view));
	}
	else
	{
		if (!fullScreenWindow)
			return;
		request.accept();
		fullScreenWindow.reset();
	}
}
