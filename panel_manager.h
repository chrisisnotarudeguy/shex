#ifndef PANEL_MANAGER_H
#define PANEL_MANAGER_H

#include <QWidget>
#include <QMap>
#include <QHBoxLayout>
#include <QEvent>
#include "events/event_types.h"
#include "panels/abstract_panel.h"

class hex_editor;

class panel_manager : public QWidget
{
		Q_OBJECT
	public:
		explicit panel_manager(QWidget *parent = 0);
		void init_displays();
		void connect_to_editor(hex_editor *editor);
		QHBoxLayout *get_layout(){ return layout; }
		
	signals:
		void send_event(QEvent *event);
		
	protected:
		virtual bool event(QEvent *event);
		
	private:
		QHBoxLayout *layout = new QHBoxLayout();
		QMap<panel_events, abstract_panel *> panel_map;
		
		abstract_panel *find_panel(panel_events id);
};

#endif // PANEL_MANAGER_H