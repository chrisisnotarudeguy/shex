#include "hex_editor.h"
#include "version.h"
#include <QPainter>
#include <QTextStream>
#include <QFontMetrics>
#include <QApplication>
#include <QAction>
#include <QMenu>
#include <cctype>
#include "QDebug"

hex_editor::hex_editor(QWidget *parent) :
    QWidget(parent)
{
	ROM.setFileName("smw.smc");
	ROM.open(QFile::ReadWrite);
	buffer = ROM.readAll();
	columns = 16;
	rows = 32;
	offset = 0;
	setFocusPolicy(Qt::WheelFocus);
	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(update_cursor()));
	timer->start(1000);
	scroll_timer = new QTimer(this);
	connect(scroll_timer, SIGNAL(timeout()), this, SLOT(auto_scroll_update()));
	cursor_state = false;
	font_setup();
	
	scroll_mode = false;
	
	total_width = column_width(get_line(0).length());
	vertical_offset = 6;
	vertical_shift = column_height(1);
	cursor_position.setY(vertical_offset);
	cursor_position.setX(column_width(11));
	is_dragging = false;
	
	this->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
	    this, SLOT(context_menu(const QPoint&)));
	
	clipboard = QApplication::clipboard();
	
	emit update_range(get_max_lines());
}

QString hex_editor::get_address(int address)
{
	int bank = (address & 0x7f8000) >> 15;
	int word = 0x8000 + (address & 0x7fff);
	return QString::number(bank, 16).rightJustified(2, '0').toUpper() +
			":" + QString::number(word,16).toUpper();
}

QSize hex_editor::minimumSizeHint() const
{
	return QSize(total_width, rows*font_height+font_height+vertical_offset+vertical_shift);
}

void hex_editor::slider_update(int position)
{
	if(!scroll_mode){
		cursor_position.setY(cursor_position.y() + (offset - position * columns));
		int old_offset = offset;
		offset = position * columns;
		if(selection_active){
			selection_start.setY(selection_start.y()  - (offset - old_offset));
			selection_current.setY(selection_current.y()  - (offset - old_offset));
		}
		update();
	}else{
		position -= height() / 2;
		if(position < 0){
			scroll_direction = false;
			position = -position;
		}else if(position > 0){
			scroll_direction = true;
		}else{
			scroll_speed = INT_MAX;
			scroll_timer->setInterval(scroll_speed);
			return;
		}
		scroll_speed = qAbs(((position - (height() /2))-1) / 15);
		scroll_timer->setInterval(scroll_speed);
	}
}

void hex_editor::auto_scroll_update()
{
	if(is_dragging){
		if(!scroll_direction){
			selection_update(mouse_position.x(), mouse_position.y());
		}else{
			selection_update(mouse_position.x(), mouse_position.y());
		}
		return;
	}
	int scroll_factor = 1;
	if(scroll_speed < 5){
		scroll_factor = qAbs(scroll_speed - 20);
	}
	for(int i = 0; i < scroll_factor; i++){
		if(!scroll_direction){
			update_cursor_position(cursor_position.x(), cursor_position.y() - font_height, false);
		}else{
			update_cursor_position(cursor_position.x(), cursor_position.y() + font_height, false);
		}
	}
	update();
}

void hex_editor::control_auto_scroll(bool enabled)
{
	auto_scrolling = enabled;
	if(auto_scrolling){
		scroll_timer->start(scroll_speed);
	}else{
		scroll_timer->stop();
	}
}

void hex_editor::update_cursor()
{
	cursor_state = !cursor_state;
	update();
}

void hex_editor::context_menu(const QPoint& position)
{	
	QMenu menu;
	menu.addAction("Cut", this, SLOT(cut()), QKeySequence::Cut)->setDisabled(!selection_active);
	menu.addAction("Copy", this, SLOT(copy()), QKeySequence::Copy)->setDisabled(!selection_active);
	menu.addAction("Paste", this, SLOT(paste()), QKeySequence::Paste)->setDisabled(check_paste_data());
	menu.addAction("Delete", this, SLOT(delete_text()), QKeySequence::Delete)->setDisabled(!selection_active);
	menu.addSeparator();
	menu.addAction("Select all", this, SLOT(select_all()), QKeySequence::SelectAll);
	menu.addSeparator();
	menu.addAction("Disassemble", this, SLOT(disassemble()))->setDisabled(!selection_active);
	
	menu.exec(mapToGlobal(position));
}

void hex_editor::cut()
{
	if(!selection_active){
		return;
	}
	int position1 = get_buffer_position(selection_start.x(), selection_start.y());
	int position2 = get_buffer_position(selection_current.x(), selection_current.y());
	if(position1 > position2){
		qSwap(position1, position2);
	}
	
	clipboard->setText(buffer.mid(position1, position2-position1).toHex());
	buffer.remove(position1, position2-position1);
	selection_active = false;
}

void hex_editor::copy()
{
	if(!selection_active){
		return;
	}
	int position1 = get_buffer_position(selection_start.x(), selection_start.y());
	int position2 = get_buffer_position(selection_current.x(), selection_current.y());
	if(position1 > position2){
		qSwap(position1, position2);
	}
	
	clipboard->setText(buffer.mid(position1, position2-position1).toHex());
}

void hex_editor::paste()
{
	if(check_paste_data()){
		return;
	}
	if(selection_active){
		int position1 = get_buffer_position(selection_start.x(), selection_start.y());
		int position2 = get_buffer_position(selection_current.x(), selection_current.y());
		if(position1 > position2){
			qSwap(position1, position2);
		}
		buffer.replace(position1, position2-position1, QByteArray::fromHex(clipboard->text().toUtf8()));
		selection_active = false;
		return;
	}
	if(cursor_position.x() % 3 != 1){
		update_cursor_position(cursor_position.x()+font_width, 
		                       cursor_position.y()-vertical_shift-font_height/2);
	}
	int position = get_buffer_position(cursor_position.x(), cursor_position.y());
	buffer.insert(position, QByteArray::fromHex(clipboard->text().toUtf8()));
	selection_active = false;
}

void hex_editor::delete_text()
{
	if(!selection_active){
		buffer.remove(get_buffer_position(cursor_position.x(), cursor_position.y()), 1);
		return;
	}
	int position1 = get_buffer_position(selection_start.x(), selection_start.y());
	int position2 = get_buffer_position(selection_current.x(), selection_current.y());
	if(position1 > position2){
		qSwap(position1, position2);
	}
	
	buffer.remove(position1, position2-position1);
	selection_active = false;
	cursor_position = selection_start;
	update();
}

void hex_editor::select_all()
{
}

void hex_editor::disassemble()
{
}

void hex_editor::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);
	painter.translate(0, vertical_shift);
	int hex_offset = column_width(11);
	
	for(int i = hex_offset; i < columns * column_width(3) + hex_offset; i += column_width(6)){
		painter.fillRect(i-1, vertical_offset, column_width(2)+2, 
		                 column_height(rows)+6, palette().color(QPalette::AlternateBase));
	}

	if(cursor_position.y() > 0 && cursor_position.y() < column_height(rows)+vertical_offset && !selection_active){
		QRect active_line(hex_offset-1, cursor_position.y()-1+vertical_offset, 
		                  columns*column_width(3)-font_width+2, font_height);
		painter.fillRect(active_line, palette().color(QPalette::Highlight));
	}
	
	if(selection_active){
		QPoint start_point = get_selection_point(selection_start);
		QPoint end_point = get_selection_point(selection_current);
		
		if(end_point.y() == start_point.y()){
			QRect starting_line(start_point.x()-1, start_point.y()-1+vertical_offset, 
			                    end_point.x() - start_point.x(), font_height);
			painter.fillRect(starting_line, palette().color(QPalette::Highlight));
		}else{
			int direction = end_point.y() > start_point.y() ? 
				    hex_offset-1-end_point.x() : 
				    columns*column_width(3)-font_width+2-end_point.x()+hex_offset;
			QRect ending_line(end_point.x()-1, end_point.y()-1+vertical_offset, 
			                  direction, font_height);
			painter.fillRect(ending_line, palette().color(QPalette::Highlight));	
			
			direction =  end_point.y() < start_point.y() ? 
			             hex_offset-1-start_point.x():
			             columns*column_width(3)-font_width+2-start_point.x()+hex_offset; 
			QRect starting_line(start_point.x()-1, start_point.y()-1+vertical_offset, 
			                    direction, font_height);
			painter.fillRect(starting_line, palette().color(QPalette::Highlight));
			if(qAbs(end_point.y()-start_point.y()) > column_height(1)){
				if(end_point.y() < start_point.y()){
					qSwap(end_point, start_point); 
				}
				QRect middle_line(hex_offset-1, start_point.y()+font_height-1+vertical_offset, 
						  columns*column_width(3)-font_width+2, 
						  end_point.y()-start_point.y()-font_height);
				painter.fillRect(middle_line, palette().color(QPalette::Highlight));
			}	
		}
	}
	
	QColor text = palette().color(QPalette::WindowText);
	painter.setPen(text);
	painter.setFont(font);

	if(cursor_state && cursor_position.y() > 0 && cursor_position.y() < column_height(rows)+vertical_offset){
		painter.fillRect(cursor_position.x(), cursor_position.y()-1+vertical_offset, 1, font_height, text);
	}

	int byte_count = rows * columns + offset;
	for(int i = offset; i < byte_count; i += columns){
		QString line = get_line(i);
		painter.drawText(0, column_height((i-offset)/columns)+font_height+vertical_offset, line);
	}

}

void hex_editor::keyPressEvent(QKeyEvent *event)
{
	
	if(event->modifiers() == Qt::ControlModifier){
		switch(event->key()){
			case Qt::Key_X:
				cut();
			break;
			case Qt::Key_C:
				copy();
			break;
			case Qt::Key_V:
				paste();
			break;
			case Qt::Key_A:
				select_all();
			break;
		}
		update();
		return;
	}
	
	if(event->modifiers() == Qt::AltModifier){
		switch(event->key()){
			case Qt::Key_S:
				scroll_mode = !scroll_mode;
				emit update_range(get_max_lines());
				emit toggle_scroll_mode(scroll_mode);
			break;
			case Qt::Key_V:
				display_version_dialog();
			break;
		}
		update();
		return;
	}
	
	if(event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9){
		if(selection_active){
			delete_text();
		}
		update_nibble(event->key() - Qt::Key_0);
	}else if(event->key() >= Qt::Key_A && event->key() <= Qt::Key_F){
		if(selection_active){
			delete_text();
		}
		update_nibble(event->key() - Qt::Key_A + 10);
	}

	switch(event->key()){
	        case Qt::Key_Delete:
			delete_text();
		break;
		case Qt::Key_Backspace:
			update_cursor_position(cursor_position.x()-column_width(3), cursor_position.y(), false);
			delete_text();
		break;

		case Qt::Key_Up:
			update_cursor_position(cursor_position.x(), cursor_position.y() - font_height);
		break;
		case Qt::Key_Down:
			update_cursor_position(cursor_position.x(), cursor_position.y() + font_height);
		break;
		case Qt::Key_Right:
			update_cursor_position(cursor_position.x()+font_width, cursor_position.y());
		break;
		case Qt::Key_Left:
			update_cursor_position(cursor_position.x()-column_width(2), cursor_position.y());
		break;
		case Qt::Key_PageUp:
			update_cursor_position(cursor_position.x(), cursor_position.y() - column_height(rows));
		break;
		case Qt::Key_PageDown:
			update_cursor_position(cursor_position.x(), cursor_position.y() + column_height(rows));
		break;
		default:
		break;
	}
}

void hex_editor::wheelEvent(QWheelEvent *event)
{
	int steps = event->delta() / 8 / 15;
	int old_offset = offset;
	if(steps > 0 && offset > 0){
		if(offset - columns * steps < 0){
			offset = 0;
			cursor_position.setY(vertical_offset);
		}else{
			offset -= columns * steps;
			cursor_position.setY(cursor_position.y()+(column_height(steps)));
		}
	}
	if(steps < 0 && offset < buffer.size()){
		if((offset + columns * -steps) > buffer.size() - columns * rows){
			offset = buffer.size() - columns * rows;
			cursor_position.setY(column_height(rows-1)+vertical_offset);
		}else{
			offset += columns * -steps;
			cursor_position.setY(cursor_position.y()-(column_height(-steps)));
		}
	}
	if(selection_active && old_offset != offset){
		selection_start.setY(selection_start.y()  - (offset - old_offset));
		selection_current.setY(selection_current.y()  - (offset - old_offset));
	}
	if(!scroll_mode){
		emit update_slider(offset / columns);
	}
}

void hex_editor::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == Qt::RightButton){
		return;
	}
		
	selection_active = false;
	if(event->y() < vertical_offset+vertical_shift){
		event->ignore();
		return;
	}
	if(event->x() > column_width(11) && event->x() < column_width(11+columns*3)-font_width){
		update_cursor_position(event->x(), event->y()-vertical_shift-font_height/2);
		if(!is_dragging){
			is_dragging = true;
			selection_start = cursor_position;
			if(cursor_position.x() % 3 != 1){
				selection_start.setX(cursor_position.x()-font_width);
			}
			selection_current = selection_start;
		}
	}
}

void hex_editor::mouseMoveEvent(QMouseEvent *event)
{
	mouse_position = event->pos();
	if(is_dragging){
		selection_active = true;
		selection_update(event->x(), event->y());
		if(event->y() > column_height(rows)){
			scroll_timer->start(20);
			scroll_direction = true;
		}else if(event->y() < vertical_shift){
			scroll_timer->start(20);
			scroll_direction = false;
		}else{
			scroll_timer->stop();
		}
	}
}

void hex_editor::mouseReleaseEvent(QMouseEvent *event)
{
	mouse_position = event->pos();
	if(is_dragging){
		selection_update(event->x(), event->y());
		scroll_timer->stop();
	}
	is_dragging = false;
	if(selection_current == selection_start){
		selection_active = false;
	}
}

void hex_editor::resizeEvent(QResizeEvent *event)
{
	Q_UNUSED(event);
	rows = (size().height() - vertical_shift - font_height)/ font_height;
	emit update_range(get_max_lines());
}

void hex_editor::font_setup()
{
	font.setFamily("Courier");
	font.setStyleHint(QFont::TypeWriter);
	font.setKerning(false);
	font.setPixelSize(14);
	
	QFontMetrics font_info(font);
	font_width = font_info.averageCharWidth();
	font_height = font_info.height();
}

void hex_editor::selection_update(int x, int y)
{
	bool override = false;
	int old_offset = offset;
	if(x < column_width(11)){
		x = column_width(11);
	}else if(x >= column_width(10+columns*3)-font_width){
		x = column_width(8+columns*3)-font_width;
		override = true;
	}
	if(y < vertical_shift && offset == 0){
		x = column_width(11);
	}
	update_cursor_position(x, y-vertical_shift-font_height/2);
	if(cursor_position.x() % 3 != 1){
		update_cursor_position(x+font_width, y-vertical_shift-font_height/2);
	}
	selection_current = cursor_position;
	if(override){
		selection_current.setX(selection_current.x()+column_width(2));
		cursor_position = selection_current;
	}
	if(offset == buffer.size() - columns * rows && y > column_height(rows)){
		selection_current.setX(column_width(11+columns*3)-font_width);
		cursor_position = selection_current;
	}
	if(!offset && y < vertical_shift){
		selection_current.setX(column_width(11));
		cursor_position = selection_current;
	}
	if(old_offset != offset){
		selection_start.setY(selection_start.y() - (offset - old_offset));
	}
}

QString hex_editor::get_line(int index)
{
	QString line;
	QTextStream string_stream(&line);
	string_stream << "$" << get_address(index) << ": ";

	int line_length = index+columns;

	if(line_length > buffer.size()){
		line_length = buffer.size();
	}

	for(int i = index; i < line_length; i++){
		string_stream << " " 
		              << QString::number((unsigned char)buffer.at(i),16).rightJustified(2, '0').toUpper();
	}

	string_stream << "    ";

	for(int i = index; i < line_length; i++){
		if(isprint((unsigned char)buffer.at(i))){
			string_stream << buffer.at(i);
		}else{
			string_stream << ".";
		}
	}

	return line;
}

QPoint hex_editor::get_selection_point(QPoint point)
{
	if(point.y() < 0){
		point.setY(vertical_offset);
		point.setX(column_width(11));
	}else if(point.y() > column_height(rows)+vertical_offset - font_height){
		point.setY(column_height(rows)+vertical_offset -font_height);
		point.setX(column_width(11+columns*3)-font_width);
	}
	return point;
}

void hex_editor::update_nibble(char byte)
{
	int position = get_buffer_position(cursor_position.x(), cursor_position.y(), false);
	buffer[position/2] = (buffer.at(position/2) &
			     ((0x0F >> ((position & 1) << 2)) | (0x0F << ((position & 1) << 2)))) |
			     (byte << (((position & 1)^1) << 2));
	update_cursor_position(cursor_position.x()+font_width, cursor_position.y());
}

void hex_editor::update_cursor_position(int x, int y, bool do_update)
{
	int x_column = x - (x % font_width);
	if(x < column_width(11)-font_width){
		if(y < vertical_offset){
			x_column = column_width(11);
		}else{
			x_column = (columns - 1) * column_width(4) - column_width(3);
		}
		y -= font_height;
	}

	if(x_column > (columns - 1) * column_width(4) - column_width(3)){
		x_column = column_width(11);
		y += font_height;
	}

	if(y > rows * font_height){
		y -= font_height;
		if(offset < buffer.size() - columns * rows){
			offset += columns;
			if(selection_active){
				selection_start.setY(selection_start.y() - columns);
				selection_current.setY(selection_current.y() - columns);
			}
			if(!scroll_mode){
				emit update_slider(offset / columns);
			}
		}else{
			x_column = (columns - 1) * column_width(4) - column_width(3);
		}
	}
	if(y < 0 && offset > 0){
		y += font_height;
		offset -= columns;
		if(selection_active){
			selection_start.setY(selection_start.y() + columns);
			selection_current.setY(selection_current.y() + columns);
		}
		if(!scroll_mode){
			emit update_slider(offset / columns);
		}
	}
	if(y > 0 && y < column_height(rows)){
		cursor_position.setY(y - (y % font_height) + vertical_offset);
	}
	if(x_column % 3 != 2){
		cursor_position.setX(x_column);
	}else{
		cursor_position.setX(x_column + font_width);
	}
	cursor_state = true;
	if(do_update){
		update();
	}
}

int hex_editor::get_buffer_position(int x, int y, bool byte_align)
{
	int position = (x - column_width(11)) / font_width;
	position -= position / 3;
	position = ((y-vertical_offset)/font_height)*columns*2+position+offset*2;
	return byte_align ? position/2 : position;

}
