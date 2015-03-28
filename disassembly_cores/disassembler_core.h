#ifndef DISASSEMBLER_CORE_H
#define DISASSEMBLER_CORE_H

#include <QGridLayout>

#include "rom_buffer.h"
#include "selection.h"

class disassembler_core : public QObject
{
	Q_OBJECT
	public:
		struct opcode{
			QString name;
		};
		using QObject::QObject;
		virtual QGridLayout *core_layout() = 0;
		virtual QString disassemble(selection selection_area, const ROM_buffer *b);
		
	protected:
		QByteArray data;
		selection region;
		const ROM_buffer *buffer;
		int delta;
		
		QString add_label(int destination);
		QString disassembly_text();
		void reset();
		void decode_name_args(QString &name);
		QString get_hex(int n, int bytes);
		unsigned int get_operand(int n);
		
		virtual QString decode_name_arg(const char arg, int &size) = 0;
		virtual QString address_to_label(int address) = 0;
		virtual opcode get_opcode(int op) = 0;
		virtual int get_base() = 0;
		virtual bool abort_unlikely(int op) = 0;
		virtual void update_state() = 0;
		virtual void set_flags(bookmark_data::types flags) = 0;
		
	private:
		struct block{
			enum data_format{
				CODE = 0,
				DATA_PACKED = 1,
				DATA_PACKED_END = 4,
				DATA_UNPACKED = 5,
				DATA_UNPACKED_END = 9
			};
			QString label = "";
			QString data = "";
			data_format format;
		};

		QMap<int, block> disassembly_list;
		int label_id;
		
		void add_data(int destination, QString data, block::data_format format);
		void make_table(QByteArray &data, int start, int size, int width, bool packed);
};

#endif // DISASSEMBLER_CORE_H
