#include "toolwidgets.h"
#include "math.h"
#include "smallUsefulFunctions.h"
#include "utilsSystem.h"
#include "configmanagerinterface.h"

void adjustScrollBar(QScrollBar *scrollBar, double factor)
{
	scrollBar->setValue(int(factor * scrollBar->value()
	                        + ((factor - 1) * scrollBar->pageStep() / 2)));
}


PreviewWidget::PreviewWidget(QWidget *parent): QScrollArea(parent)
{
	setBackgroundRole(QPalette::Base);

	mCenter = ConfigManagerInterface::getInstance()->getOption("Preview/PreviewPanelCenter", true).toBool();
	mFit = ConfigManagerInterface::getInstance()->getOption("Preview/PreviewPanelFit", false).toBool();

	preViewer = new QLabel(this);
	preViewer->setBackgroundRole(QPalette::Base);
	preViewer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	preViewer->setScaledContents(true);
	preViewer->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(preViewer, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenu(QPoint)));
	connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenu(QPoint)));
	setContextMenuPolicy(Qt::CustomContextMenu);

	pvscaleFactor = 1.0;

	setWidget(preViewer);
}

void PreviewWidget::scaleImage(double factor)
{
	REQUIRE(preViewer->pixmap());
	pvscaleFactor *= factor;
	preViewer->resize(pvscaleFactor * preViewer->pixmap()->size());

	adjustScrollBar(horizontalScrollBar(), factor);
	adjustScrollBar(verticalScrollBar(), factor);

	mFit = false;
	ConfigManagerInterface::getInstance()->setOption("Preview/PreviewPanelFit", mFit);
}

void PreviewWidget::previewLatex(const QPixmap &previewImage)
{
	preViewer->setPixmap(previewImage);
	preViewer->adjustSize();
	pvscaleFactor = 1.0;
	if (mFit) fitImage(true);
	if (mCenter) centerImage(true);
}

void PreviewWidget::fitImage(bool fit)
{
	mFit = fit;
	ConfigManagerInterface::getInstance()->setOption("Preview/PreviewPanelFit", mFit);
	REQUIRE(preViewer->pixmap());
	if (fit) {
		// needs to be improved
		QSize m_size = size() - QSize(2, 2);
		QSize m_labelSize = preViewer->size();
		qreal ratio = 1.0 * m_labelSize.height() / m_labelSize.width();
		qreal ratioPreviewer = 1.0 * m_size.height() / m_size.width();
		int h, w;
		if (ratioPreviewer > ratio) {
			h = qRound(ratio * m_size.width());
			w = m_size.width();
			pvscaleFactor = 1.0 * w / preViewer->pixmap()->size().width();
		} else {
			h = m_size.height();
			w = qRound(m_size.height() / ratio);
			pvscaleFactor = 1.0 * h / preViewer->pixmap()->size().height();;
		}
		preViewer->resize(w, h);
		//setWidgetResizable(true);
	} else {
		resetZoom();
	}
}

void PreviewWidget::centerImage(bool center)
{
	mCenter = center;
	ConfigManagerInterface::getInstance()->setOption("Preview/PreviewPanelCenter", mCenter);
	if (mCenter) setAlignment(Qt::AlignCenter);
	else setAlignment(Qt::AlignLeft | Qt::AlignTop);
}

void PreviewWidget::zoomOut()
{
	scaleImage(1 / 1.4);
}

void PreviewWidget::zoomIn()
{
	scaleImage(1.4);
}

void PreviewWidget::resetZoom()
{
	pvscaleFactor = 1.0;
	scaleImage(1.0);
}

void PreviewWidget::wheelEvent(QWheelEvent *event)
{
	if (!preViewer->pixmap()) return;
	if (event->modifiers() == Qt::ControlModifier) {
		float numDegrees = event->delta() / 8.0f;
		float numSteps = numDegrees / 15.0f;
		scaleImage(pow(1.4, numSteps));
		event->accept();
	} else QScrollArea::wheelEvent(event);
}

void PreviewWidget::contextMenu(QPoint point)
{
	if (!preViewer->pixmap()) return;
	QMenu menu;
	menu.addAction(tr("Zoom In"), this, SLOT(zoomIn()));
	menu.addAction(tr("Zoom Out"), this, SLOT(zoomOut()));
	menu.addAction(tr("Reset Zoom"), this, SLOT(resetZoom()));
	QAction *act = menu.addAction(tr("Fit"));
	act->setCheckable(true);
	act->setChecked(mFit);
	connect(act, SIGNAL(triggered(bool)), this, SLOT(fitImage(bool)));
	act = menu.addAction(tr("Center"));
	act->setCheckable(true);
	act->setChecked(mCenter);
	connect(act, SIGNAL(triggered(bool)), this, SLOT(centerImage(bool)));
	QWidget *menuParent = qobject_cast<QWidget *>(sender());
	Q_ASSERT(menuParent);
	if (!menuParent) menuParent = preViewer;
	menu.exec(menuParent->mapToGlobal(point));
}

OutputViewWidget::OutputViewWidget(QWidget *parent) :
	TitledPanel(parent),
	MESSAGES_PAGE("messages"),
	LOG_PAGE("log"),
	PREVIEW_PAGE("preview"),
	SEARCH_RESULT_PAGE("search")
{
	setSelectorStyle(TabSelector);
	mToggleViewAction->setText(tr("Messages / Log File"));
	mToggleViewAction->setIcon(getRealIcon("logpanel"));


	// messages
	QFontMetrics fm(QApplication::font());
	OutputMessages = new LogEditor(this);
	OutputMessages->setFocusPolicy(Qt::ClickFocus);
	OutputMessages->setMinimumHeight(3 * (fm.lineSpacing() + 4));
	OutputMessages->setReadOnly(true);

	appendPage(new TitledPanelPage(OutputMessages, MESSAGES_PAGE, tr("Messages")), false);

	// log / errors
	logWidget = new LatexLogWidget(this);

	TitledPanelPage *page = new TitledPanelPage(logWidget, LOG_PAGE, tr("Log"));
	page->addToolbarActions(logWidget->displayActions());
	appendPage(page, false);

	// previewer
	previewWidget = new PreviewWidget(this);
	appendPage(new TitledPanelPage(previewWidget, PREVIEW_PAGE, tr("Preview")), false);

	// global search results

	searchResultWidget = new SearchResultWidget(this);

	//appendPage(new TitledPanelPage(OutputSearchTree, SEARCH_RESULT_PAGE, tr("Search Results")));
	appendPage(new TitledPanelPage(searchResultWidget, SEARCH_RESULT_PAGE, tr("Search Results")));
}

void OutputViewWidget::previewLatex(const QPixmap &pixmap)
{
	previewWidget->previewLatex(pixmap);
	//showPreview();
}

bool OutputViewWidget::isPreviewPanelVisible()
{
	if (!isVisible()) return false;
	return currentPageId() == PREVIEW_PAGE;
}

void OutputViewWidget::setMessage(const QString &message)
{
	setCurrentPage(MESSAGES_PAGE);
	OutputMessages->setText(message);
}

void OutputViewWidget::insertMessageLine(const QString &message)
{
	OutputMessages->insertLine(message);
}

void OutputViewWidget::copy()
{
	QString pageId = currentPageId();

	if (pageId == LOG_PAGE) {
		logWidget->copy();
	} else if (pageId == MESSAGES_PAGE) {
		OutputMessages->copy();
	}
}

void OutputViewWidget::resetMessages(bool noTabChange)
{
	OutputMessages->clear();
	if (!noTabChange) setCurrentPage(MESSAGES_PAGE);
}

void OutputViewWidget::resetMessagesAndLog(bool noTabChange)
{
	resetMessages(noTabChange);
	logWidget->resetLog();
}
void OutputViewWidget::selectLogEntry(int logEntryNumber, bool makeVisible)
{
	if (!logWidget->logEntryNumberValid(logEntryNumber)) return;
	if (makeVisible) showPage(LOG_PAGE);
	logWidget->selectLogEntry(logEntryNumber);
}

void OutputViewWidget::retranslateUi()
{
	mToggleViewAction->setText(tr("Messages / Log File"));
	pageFromId(MESSAGES_PAGE)->setTitle(tr("Messages"));
	pageFromId(LOG_PAGE)->setTitle(tr("Log"));
	pageFromId(PREVIEW_PAGE)->setTitle(tr("Preview"));
	pageFromId(SEARCH_RESULT_PAGE)->setTitle(tr("Search Results"));
	updateTopbar();
}

/*
void OutputViewWidget::gotoLogLine(int logLine){
	logWidget->gotoLogEntry(logModel->logLineNumberToLogEntryNumber(logLine));
}*/

bool OutputViewWidget::childHasFocus()
{
	return logWidget->childHasFocus()
	       || OutputMessages->hasFocus();
}

void OutputViewWidget::changeEvent(QEvent *event)
{
	switch (event->type()) {
	case QEvent::LanguageChange:
		retranslateUi();
		break;
	default:
		break;
	}
}


//====================================================================
// CustomWidgetList (for left panel)
//====================================================================

Q_DECLARE_METATYPE(QAction *)

CustomWidgetList::CustomWidgetList(QWidget *p):
	QDockWidget(p), newStyle(false), toolbox(0), frame(0), stack(0), toolbar(0)
{
	toggleViewAction()->setIcon(getRealIcon("sidebar"));
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customContextMenuRequested(QPoint)));
}

void CustomWidgetList::addWidget(QWidget *widget, const QString &id, const QString &text, const QString &iconName)
{
	widgets << widget;
	widget->setProperty("id", id);
	widget->setProperty("Name", text);
	widget->setProperty("iconName", iconName);
	widget->setProperty("StructPos", widgets.size());

	QAction *Act = new QAction(text, this);
	Act->setCheckable(true);
	Act->setChecked(!hiddenWidgetsIds.contains(id));
	Act->setData(id);
	connect(Act, SIGNAL(toggled(bool)), this, SLOT(toggleWidgetFromAction(bool)));
	addAction(Act);
}

void CustomWidgetList::setWidgetText(const QString &id, const QString &text)
{
	setWidgetText(widget(id), text);
}
void CustomWidgetList::setWidgetText(QWidget *widget, const QString &text)
{
	int pos = widgets.indexOf(widget);
	if (pos < 0) return;
	widget->setProperty("Name", text);
	if (newStyle) actions()[pos]->setToolTip(text);
	else toolbox->setItemText(pos, text);
}

void CustomWidgetList::setWidgetIcon(const QString &id, const QString &icon)
{
	setWidgetIcon(widget(id), icon);
}

void CustomWidgetList::setWidgetIcon(QWidget *widget, const QString &icon)
{
	int pos = widgets.indexOf(widget);
	if (pos < 0) return;
	widget->setProperty("iconName", icon);
}

void CustomWidgetList::showPageFromAction()
{
	QAction *act = qobject_cast<QAction *>(sender());
	if (!act) return;
	QWidget *wid = widget(act->data().toString());
	stack->setCurrentWidget(wid);
	setWindowTitle(act->toolTip());
	foreach (QAction *a, toolbar->actions())
		a->setChecked(a == act);
}

void CustomWidgetList::currentWidgetChanged(int i)
{
	Q_ASSERT(newStyle == false);
	setWindowTitle(toolbox->itemText(i));
}

void CustomWidgetList::toggleWidgetFromAction(bool on)
{
	QAction *act = qobject_cast<QAction *>(sender());
	if (!act || act->data().toString() == "") return;
	if (on)
		hiddenWidgetsIds.removeAll(act->data().toString());
	else if (!hiddenWidgetsIds.contains(act->data().toString()))
		hiddenWidgetsIds.append(act->data().toString());
	showWidgets(newStyle);
}

void CustomWidgetList::customContextMenuRequested(const QPoint &localPosition)
{
	QWidget *widget = currentWidget();
	if (widget && widget->underMouse()) //todo?: use a more reliable function than underMouse (see qt bug 260000)
		emit widgetContextMenuRequested(widget, mapToGlobal(localPosition));
	else {
		QMenu menu;
		menu.addActions(actions());
		menu.exec(mapToGlobal(localPosition));
	}
}

void CustomWidgetList::showWidgets(bool newLayoutStyle)
{
	if (toolbox) {
		for (int i = 0; i < widgets.count(); i++) {
			toolbox->removeItem(toolbox->indexOf(widgets[i]));
			widgets[i]->setParent(this);//otherwise it will be deleted
		}
		delete toolbox;
	}
	if (stack) {
		for (int i = 0; i < widgets.count(); i++) {
			stack->removeWidget(widgets[i]);
			widgets[i]->setParent(this);//otherwise it will be deleted
		}
		delete stack;
	}
	if (toolbar) delete toolbar;
	if (frame) delete frame;
	newStyle = newLayoutStyle;
	if (newLayoutStyle) {
		toolbox = 0;
		frame = new QFrame(this);
		frame->setLineWidth(0);
		frame->setFrameShape(QFrame::Box);
		frame->setFrameShadow(QFrame::Plain);

		toolbar = new QToolBar("LogToolBar", this);
		toolbar->setFloatable(false);
		toolbar->setOrientation(Qt::Vertical);
		toolbar->setMovable(false);

		setToolbarIconSize(ConfigManagerInterface::getInstance()->getOption("GUI/SecondaryToobarIconSize").toInt());

		stack = new QStackedWidget(this);

		for (int i = 0; i < widgets.size(); i++)
			if (!hiddenWidgetsIds.contains(widgetId(widgets[i]))) {
				stack->addWidget(widgets[i]);
				QAction *act = toolbar->addAction(QIcon(widgets[i]->property("iconName").toString()), widgets[i]->property("Name").toString());
				act->setCheckable(true);
				if (i == 0) act->setChecked(true);
				act->setData(widgetId(widgets[i]));
				connect(act, SIGNAL(triggered()), this, SLOT(showPageFromAction()));
				widgets[i]->setProperty("associatedAction", QVariant::fromValue<QAction *>(act));
			} else widgets[i]->hide();

		QHBoxLayout *hlayout = new QHBoxLayout(frame);
		hlayout->setSpacing(0);
		hlayout->setMargin(0);
		hlayout->addWidget(toolbar);
		hlayout->addWidget(stack);

		setWidget(frame);
	} else {
		frame = 0;
		toolbar = 0;
		stack = 0;
		toolbox = new QToolBox(this);
		for (int i = 0; i < widgets.size(); i++)
			if (!hiddenWidgetsIds.contains(widgetId(widgets[i]))) {
				toolbox->addItem(widgets[i], QIcon(widgets[i]->property("iconName").toString()), widgets[i]->property("Name").toString());
			} else widgets[i]->hide();
		connect(toolbox, SIGNAL(currentChanged(int)), SLOT(currentWidgetChanged(int)));
		setWidget(toolbox);

	}
	if (!widgets.empty()) //name after active (first) widget
		setWindowTitle(widgets.first()->property("Name").toString());
}

void CustomWidgetList::setToolbarIconSize(int sz)
{
	if (toolbar) {
		toolbar->setIconSize(QSize(sz, sz));
	}
}

int CustomWidgetList::widgetCount() const
{
	return widgets.count();
}

void CustomWidgetList::setHiddenWidgets(const QString &hidden)
{
	hiddenWidgetsIds = hidden.split("|");
}

QString CustomWidgetList::hiddenWidgets() const
{
	return hiddenWidgetsIds.join("|");
}

/*'void CustomWidgetList::addWidgetOld(QWidget* widget, const QString& text, const QIcon& icon){
}
void CustomWidgetList::addWidgetNew(QWidget* widget, const QString& text, const QIcon& icon){
	stack->addWidget(*list);
	toolbar->addAction(icon,text);
}*/

QWidget *CustomWidgetList::widget(int i) const
{
	return widgets[i];
}

QWidget *CustomWidgetList::widget(const QString &id) const
{
	for (int i = 0; i < widgets.size(); i++)
		if (widgetId(widgets[i]) == id)
			return widgets[i];
	return 0;
}

QList<QWidget *> CustomWidgetList::getWidgets() const
{
	return widgets;
}

void CustomWidgetList::setCurrentWidget(QWidget *widget)
{
	if (newStyle) {
		stack->setCurrentWidget(widget);
		QAction *act = widget->property("associatedAction").value<QAction *>();
		foreach (QAction *a, toolbar->actions())
			a->setChecked(a == act);
	} else {
		toolbox->setCurrentWidget(widget);
	}
}
QWidget *CustomWidgetList::currentWidget() const
{
	if (newStyle) return stack->currentWidget();
	else return toolbox->currentWidget();
}

bool CustomWidgetList::isNewLayoutStyleEnabled() const
{
	return newStyle;
}

QString CustomWidgetList::widgetId(QWidget *widget) const
{
	if (!widget) return "";
	return widget->property("id").toString();
}
