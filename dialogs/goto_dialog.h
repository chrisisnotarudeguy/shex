#ifndef GOTO_DIALOG_H
#define GOTO_DIALOG_H

#include <QRadioButton>

#include "abstract_dialog.h"

class goto_dialog : public abstract_dialog
{
		Q_OBJECT
	public:
		explicit goto_dialog(QWidget *parent);
		
	signals:
		void triggered(int address);
		
	public slots:
		void address_entered();
		
	private:
		QLabel *label = new QLabel("&Goto SNES offset: ", this);
		QLineEdit *offset_input = new QLineEdit(this);
		QRadioButton *absolute = new QRadioButton("Absolute offset", this);
		QRadioButton *relative = new QRadioButton("Relative offset", this);
		
		QPushButton *goto_offset = new QPushButton("Goto offset", this);
		QPushButton *close = new QPushButton("Close", this);
};

#endif // GOTO_DIALOG_H
