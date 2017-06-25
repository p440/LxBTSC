#include "QtGuiClass.h"

QtGuiClass::QtGuiClass(QWidget *parent)
	: QWidget(parent)
{
	setupUi(this);
	//QObject::connect(tabWidget, &QTabWidget::currentChanged, this, &QtGuiClass::tabSelected);
	//QObject::connect(plainTextEdit, &TsTextEdit::textSend, this, &QtGuiClass::textGet);
	tabs = new QMap<int, TsChatTabWidget*>();
	addTab("Server", -2);
	addTab("Channel", -1);
}

QtGuiClass::QtGuiClass(unsigned long long server, QString path)
	: QWidget()
{
	setupUi(this);
	//QObject::connect(tabWidget, &QTabWidget::currentChanged, this, &QtGuiClass::tabSelected);
	//QObject::connect(plainTextEdit, &TsTextEdit::textSend, this, &QtGuiClass::textGet);
	serverID = server;
	pathToPage = QString("file:///%1LxBTSC/template/chat.html").arg(path);
	tabs = new QMap<int, TsChatTabWidget*>();
	addTab("Server", -2);
	addTab("Channel", -1);
}

QtGuiClass::~QtGuiClass()
{
}

void QtGuiClass::setupUi(QWidget *QtGuiClass)
{
	if (QtGuiClass->objectName().isEmpty())
		QtGuiClass->setObjectName(QStringLiteral("QtGuiClass"));
	QtGuiClass->resize(636, 534);
	verticalLayout = new QVBoxLayout(QtGuiClass);
	verticalLayout->setSpacing(1);
	verticalLayout->setContentsMargins(1, 1, 1, 1);
	verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
	stack = new QStackedWidget(QtGuiClass);
	stack->setObjectName(QStringLiteral("myStack"));
	
	//tabWidget = new QTabWidget(QtGuiClass);
	//tabWidget->setObjectName(QStringLiteral("tabWidget"));
	//tabWidget->setTabPosition(QTabWidget::South);
	
	verticalLayout->addWidget(stack);
	//verticalLayout->addWidget(tabWidget);

	//plainTextEdit = new TsTextEdit(QtGuiClass);
	//plainTextEdit->setObjectName(QStringLiteral("plainTextEdit"));
	//plainTextEdit->setEnabled(true);
	//QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	//sizePolicy.setHorizontalStretch(0);
	//sizePolicy.setVerticalStretch(120);
	//sizePolicy.setHeightForWidth(plainTextEdit->sizePolicy().hasHeightForWidth());
	//plainTextEdit->setSizePolicy(sizePolicy);
	//plainTextEdit->setMinimumSize(QSize(0, 24));
	//plainTextEdit->setMaximumSize(QSize(16777215, 120));
	//plainTextEdit->setSizeIncrement(QSize(0, 24));
	//plainTextEdit->setBaseSize(QSize(0, 0));
	//plainTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//plainTextEdit->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

	//verticalLayout->addWidget(plainTextEdit);

	QtGuiClass->setWindowTitle(QApplication::translate("QtGuiClass", "QtGuiClass", 0));
	//plainTextEdit->setPlainText(QString());

	stack->setCurrentIndex(0);
	//tabWidget->setCurrentIndex(0);

	QMetaObject::connectSlotsByName(QtGuiClass);
}

TsTextEdit* QtGuiClass::getTextEdit()
{
	//return plainTextEdit;
	return NULL;
}

void QtGuiClass::tabSelected(int index)
{
	//tabIndex = tabWidget->currentWidget()->property("chatindex").toInt();
	stack->setCurrentIndex(index);
	tabIndex = stack->currentWidget()->property("chatindex").toInt();
}

void QtGuiClass::tabLoaded(int index)
{
	foreach(QString s, tabs->value(index)->getBuffer())
	{
		tabs->value(index)->page()->runJavaScript(s);
	}
	tabs->value(index)->isLoaded = true;
}

void QtGuiClass::textGet(QString text)
{
	emit tsTextMessage(text, tabIndex);
}

void QtGuiClass::messageReceived2(QString s, int id)
{
	QString js = "AddServerLine('<div>" + s + "</div>');";

	if (tabs->contains(id))
	{
		tabs->value(id)->page()->runJavaScript(js);
	}
	else
	{
		addTab("Private", id);
		tabs->value(id)->addLine(js);
	}
}

void QtGuiClass::addTab(QString name, int id)
{
	TsTabWidget *tab = new TsTabWidget();
	QObject::connect(tab, &TsTabWidget::tabLoaded, this, &QtGuiClass::tabLoaded);
	tab->setProperty("chatindex", id);
	tab->setObjectName(QStringLiteral("tab"));
	QVBoxLayout *verticalLayout_2 = new QVBoxLayout(tab);
	verticalLayout_2->setSpacing(1);
	verticalLayout_2->setContentsMargins(1, 1, 1, 1);
	verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));

	TsChatTabWidget *webEngineView = new TsChatTabWidget(tab);

	// for some reason this causes a crash on exit?
	webEngineView->setPage(new TsWebEnginePage());
	
	QObject::connect(webEngineView, &TsChatTabWidget::loadFinished, tab, &TsTabWidget::browserLoaded);
	webEngineView->setObjectName(QStringLiteral("webEngineView"));
	//webEngineView->setUrl(QUrl(QStringLiteral("file:///C:/Users/TURSAS/AppData/Roaming/TS3Client/plugins/LxBTSC/template/chat.html")));
	webEngineView->setUrl(QUrl(pathToPage));
	verticalLayout_2->addWidget(webEngineView);
	tabs->insert(id, webEngineView);
	stack->addWidget(tab);
	//tabWidget->addTab(tab, name);
}