/* This file is part of Clementine.

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "smartplaylistsearchterm.h"
#include "smartplaylistsearchtermwidget.h"
#include "ui_smartplaylistsearchtermwidget.h"
#include "core/utilities.h"
#include "playlist/playlist.h"
#include "playlist/playlistdelegates.h"
#include "ui/iconloader.h"

#include <QFile>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QtDebug>

// Exported by QtGui
void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

class SmartPlaylistSearchTermWidget::Overlay : public QWidget {
public:
  Overlay(SmartPlaylistSearchTermWidget* parent);
  void Grab();
  void SetOpacity(float opacity);
  float opacity() const { return opacity_; }

  static const int kSpacing;
  static const int kIconSize;

protected:
  void paintEvent(QPaintEvent*);
  void mouseReleaseEvent(QMouseEvent*);

private:
  SmartPlaylistSearchTermWidget* parent_;

  float opacity_;
  QString text_;
  QPixmap pixmap_;
  QPixmap icon_;
};

const int SmartPlaylistSearchTermWidget::Overlay::kSpacing = 6;
const int SmartPlaylistSearchTermWidget::Overlay::kIconSize = 22;


SmartPlaylistSearchTermWidget::SmartPlaylistSearchTermWidget(LibraryBackend* library, QWidget* parent)
  : QWidget(parent),
    ui_(new Ui_SmartPlaylistSearchTermWidget),
    library_(library),
    overlay_(NULL),
    animation_(new QPropertyAnimation(this, "overlay_opacity", this)),
    active_(true)
{
  ui_->setupUi(this);
  connect(ui_->field, SIGNAL(currentIndexChanged(int)), SLOT(FieldChanged(int)));

  // Populate the combo boxes
  for (int i=0 ; i<SmartPlaylistSearchTerm::FieldCount ; ++i) {
    ui_->field->addItem(SmartPlaylistSearchTerm::FieldName(SmartPlaylistSearchTerm::Field(i)));
    ui_->field->setItemData(i, i);
  }
  ui_->field->model()->sort(0);

  // Icons on the buttons
  ui_->remove->setIcon(IconLoader::Load("list-remove"));

  // Set stylesheet
  QFile stylesheet_file(":/smartplaylistsearchterm.css");
  stylesheet_file.open(QIODevice::ReadOnly);
  QString stylesheet = QString::fromAscii(stylesheet_file.readAll());
  const QColor base(222, 97, 97, 128);
  stylesheet.replace("%light2", Utilities::ColorToRgba(base.lighter(140)));
  stylesheet.replace("%light",  Utilities::ColorToRgba(base.lighter(120)));
  stylesheet.replace("%dark",   Utilities::ColorToRgba(base.darker(120)));
  stylesheet.replace("%base",   Utilities::ColorToRgba(base));
  setStyleSheet(stylesheet);
}

SmartPlaylistSearchTermWidget::~SmartPlaylistSearchTermWidget() {
  delete ui_;
}

void SmartPlaylistSearchTermWidget::FieldChanged(int index) {
  SmartPlaylistSearchTerm::Field field = SmartPlaylistSearchTerm::Field(
        ui_->field->itemData(index).toInt());
  SmartPlaylistSearchTerm::Type type = SmartPlaylistSearchTerm::TypeOf(field);

  // Populate the operator combo box
  ui_->op->clear();
  foreach (SmartPlaylistSearchTerm::Operator op, SmartPlaylistSearchTerm::OperatorsForType(type)) {
    ui_->op->addItem(SmartPlaylistSearchTerm::OperatorText(type, op));
  }

  // Show the correct value editor
  QWidget* page = NULL;
  switch (type) {
    case SmartPlaylistSearchTerm::Type_Time:   page = ui_->page_time;   break;
    case SmartPlaylistSearchTerm::Type_Number: page = ui_->page_number; break;
    case SmartPlaylistSearchTerm::Type_Date:   page = ui_->page_date;   break;
    case SmartPlaylistSearchTerm::Type_Rating: page = ui_->page_number; break; // TODO
    case SmartPlaylistSearchTerm::Type_Text:   page = ui_->page_text;   break;
  }
  ui_->value_stack->setCurrentWidget(page);

  // Maybe set a tag completer
  switch (field) {
  case SmartPlaylistSearchTerm::Field_Artist:
    new TagCompleter(library_, Playlist::Column_Artist, ui_->value_text);
    break;

  case SmartPlaylistSearchTerm::Field_Album:
    new TagCompleter(library_, Playlist::Column_Album, ui_->value_text);
    break;

  default:
    ui_->value_text->setCompleter(NULL);
  }
}

void SmartPlaylistSearchTermWidget::SetActive(bool active) {
  active_ = active;
  delete overlay_;
  overlay_ = NULL;

  if (!active) {
    overlay_ = new Overlay(this);
  }
}

void SmartPlaylistSearchTermWidget::enterEvent(QEvent*) {
  if (!overlay_)
    return;

  animation_->stop();
  animation_->setEndValue(1.0);
  animation_->setDuration(80);
  animation_->start();
}

void SmartPlaylistSearchTermWidget::leaveEvent(QEvent*) {
  if (!overlay_)
    return;

  animation_->stop();
  animation_->setEndValue(0.0);
  animation_->setDuration(160);
  animation_->start();
}

void SmartPlaylistSearchTermWidget::resizeEvent(QResizeEvent* e) {
  QWidget::resizeEvent(e);
  if (overlay_ && overlay_->isVisible()) {
    QTimer::singleShot(0, this, SLOT(Grab()));
  }
}

void SmartPlaylistSearchTermWidget::showEvent(QShowEvent* e) {
  QWidget::showEvent(e);
  if (overlay_) {
    QTimer::singleShot(0, this, SLOT(Grab()));
  }
}

void SmartPlaylistSearchTermWidget::Grab() {
  overlay_->Grab();
}

void SmartPlaylistSearchTermWidget::set_overlay_opacity(float opacity) {
  if (overlay_)
    overlay_->SetOpacity(opacity);
}

float SmartPlaylistSearchTermWidget::overlay_opacity() const {
  return overlay_ ? overlay_->opacity() : 0.0;
}



SmartPlaylistSearchTermWidget::Overlay::Overlay(SmartPlaylistSearchTermWidget* parent)
  : QWidget(parent),
    parent_(parent),
    opacity_(0.0),
    text_(tr("Add search term")),
    icon_(IconLoader::Load("list-add").pixmap(kIconSize))
{
  raise();
  setCursor(Qt::PointingHandCursor);
}

void SmartPlaylistSearchTermWidget::Overlay::SetOpacity(float opacity) {
  opacity_ = opacity;
  update();
}

void SmartPlaylistSearchTermWidget::Overlay::Grab() {
  hide();

  // Take a "screenshot" of the window
  QPixmap pixmap = QPixmap::grabWidget(parent_);
  QImage image = pixmap.toImage();

  // Blur it
  QImage blurred(image.size(), QImage::Format_ARGB32_Premultiplied);
  blurred.fill(Qt::transparent);

  QPainter blur_painter(&blurred);
  qt_blurImage(&blur_painter, image, 10.0, true, false);
  blur_painter.end();

  pixmap_ = QPixmap::fromImage(blurred);

  resize(parent_->size());
  show();
  update();
}

void SmartPlaylistSearchTermWidget::Overlay::paintEvent(QPaintEvent*) {
  QPainter p(this);

  // Background
  p.fillRect(rect(), palette().window());

  // Blurred parent widget
  p.setOpacity(0.25 + opacity_ * 0.25);
  p.drawPixmap(0, 0, pixmap_);

  // Geometry
  const QSize contents_size(kIconSize + kSpacing + fontMetrics().width(text_),
                            qMax(kIconSize, fontMetrics().height()));
  const QRect contents(QPoint((width() - contents_size.width()) / 2,
                              (height() - contents_size.height()) / 2),
                       contents_size);
  const QRect icon(contents.topLeft(), QSize(kIconSize, kIconSize));
  const QRect text(icon.right() + kSpacing, icon.top(),
                   contents.width() - kSpacing - kIconSize,
                   contents.height());

  // Icon and text
  p.setOpacity(1.0);
  p.drawPixmap(icon, icon_);
  p.drawText(text, text_);
}

void SmartPlaylistSearchTermWidget::Overlay::mouseReleaseEvent(QMouseEvent*) {
  emit parent_->Clicked();
}
