#ifndef ISA_SPC700_H
#define ISA_SPC700_H

#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QSet>

#include "disassembler_core.h"

class isa_spc700 : public disassembler_core
{
		Q_OBJECT
	public:
		explicit isa_spc700(QObject *parent = 0);
		~isa_spc700();
		QGridLayout *core_layout();
		static QString id(){ return "SPC700"; }

	public slots:
		void toggle_error_stop(bool state){ error_stop = state; }
		void update_base(QString new_base);

	protected:
		QString decode_name_arg(const char arg, int &size);
		QString address_to_label(int address);
		QString format_data_value(int size, int value, bool is_pointer);
		opcode get_opcode(int op);
		int get_base();
		bool abort_unlikely(int op);
		void update_state(){}
		void set_flags(bookmark_data::types type){ Q_UNUSED(type); }
	private:		
		bool error_stop = false;
		QCheckBox *stop = new QCheckBox("Stop on unlikely");
		QLineEdit *base_input = new QLineEdit("0500");
		QLabel *base_text = new QLabel("Base address");
		static const QList<disassembler_core::opcode> opcode_list;
		static const QSet<unsigned char> unlikely;
		
		unsigned int base = 0x0500;
		
};

#endif // ISA_SPC700_H
