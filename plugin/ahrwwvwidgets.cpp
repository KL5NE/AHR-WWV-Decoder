///////////////////////////////////////////////////////////////////////////////////
// AHR WWV/WWVH Time Decoder - SDRangel receive channel plugin                   //
// Copyright (C) 2026 KL5NE, Alaska HamRadio                                     //
//                                                                               //
// Structure follows SDRangel's Radio Clock channel plugin,                      //
// (C) Edouard Griffiths F4EXB, Jon Beniston M7RCE and contributors.             //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
///////////////////////////////////////////////////////////////////////////////////

#include <QPainter>
#include <QHelpEvent>
#include <QToolTip>

#include "ahrwwvwidgets.h"

using ahrwwv::Symbol;

// ---------------------------------------------------------------- time code strip

AHRWWVTimeCodeStrip::AHRWWVTimeCodeStrip(QWidget *parent) :
    QWidget(parent),
    m_current(-1)
{
    m_have.fill(false);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize AHRWWVTimeCodeStrip::sizeHint() const { return QSize(360, 6 * 20 + 4); }
QSize AHRWWVTimeCodeStrip::minimumSizeHint() const { return QSize(260, 6 * 16 + 4); }

void AHRWWVTimeCodeStrip::setFrame(const std::array<ahrwwv::SecondSymbol, 60>& frame, int currentPosition)
{
    m_frame = frame;

    for (int i = 0; i < 60; i++) {
        // DecoderStatus::frame entries that have not been filled this minute keep framePosition == -1
        m_have[i] = frame[i].framePosition == i;
    }

    m_current = currentPosition;
    update();
}

void AHRWWVTimeCodeStrip::clear()
{
    m_have.fill(false);
    m_current = -1;
    update();
}

QRect AHRWWVTimeCodeStrip::cellRect(int second) const
{
    const int labelW = 22;
    int rows = 6, cols = 10;
    int w = (width() - labelW - 2) / cols;
    int h = (height() - 2) / rows;
    int r = second / cols, c = second % cols;
    return QRect(labelW + c * w, 1 + r * h, w - 2, h - 2);
}

int AHRWWVTimeCodeStrip::cellAt(const QPoint& p) const
{
    for (int s = 0; s < 60; s++) {
        if (cellRect(s).contains(p)) {
            return s;
        }
    }

    return -1;
}

QColor AHRWWVTimeCodeStrip::fieldColor(int s)
{
    if (s == 0) return QColor(70, 70, 70);                       // frame reference
    if (s % 10 == 9) return QColor(110, 60, 150);               // position markers
    if (s >= 4 && s <= 7) return QColor(40, 90, 140);           // year units
    if (s >= 51 && s <= 54) return QColor(40, 90, 140);         // year tens
    if ((s >= 10 && s <= 13) || (s >= 15 && s <= 17)) return QColor(40, 120, 80);  // minutes
    if ((s >= 20 && s <= 23) || s == 25 || s == 26) return QColor(140, 110, 30);   // hours
    if ((s >= 30 && s <= 33) || (s >= 35 && s <= 38) || s == 40 || s == 41) return QColor(150, 70, 40); // day of year
    if (s == 50 || (s >= 56 && s <= 58)) return QColor(30, 120, 130); // DUT1
    if (s == 2 || s == 3 || s == 55) return QColor(130, 50, 90);      // DST / leap second flags
    return QColor(55, 55, 55);                                  // unused (always 0)
}

QString AHRWWVTimeCodeStrip::describeSecond(int s)
{
    static const int w4[] = {1, 2, 4, 8};
    auto bit = [&](const char *field, int first) { return QString("%1, weight %2").arg(field).arg(w4[s - first]); };

    if (s == 0) return "Frame reference (no pulse)";
    if (s % 10 == 9) return QString("Position marker P%1").arg(s == 59 ? 0 : (s + 1) / 10);
    if (s == 2) return "DST2 (DST in effect at 24:00 UTC today)";
    if (s == 3) return "Leap second warning";
    if (s >= 4 && s <= 7) return bit("Year units", 4);
    if (s >= 10 && s <= 13) return bit("Minute units", 10);
    if (s >= 15 && s <= 17) return QString("Minute tens, weight %1").arg(QList<int>({10, 20, 40}).at(s - 15));
    if (s >= 20 && s <= 23) return bit("Hour units", 20);
    if (s == 25 || s == 26) return QString("Hour tens, weight %1").arg(s == 25 ? 10 : 20);
    if (s >= 30 && s <= 33) return bit("Day-of-year units", 30);
    if (s >= 35 && s <= 38) return QString("Day-of-year tens, weight %1").arg(QList<int>({10, 20, 40, 80}).at(s - 35));
    if (s == 40 || s == 41) return QString("Day-of-year hundreds, weight %1").arg(s == 40 ? 100 : 200);
    if (s == 50) return "UT1 sign (1 = add DUT1)";
    if (s >= 51 && s <= 54) return QString("Year tens, weight %1").arg(QList<int>({10, 20, 40, 80}).at(s - 51));
    if (s == 55) return "DST1 (DST in effect at 00:00 UTC today)";
    if (s >= 56 && s <= 58) return QString("DUT1 magnitude, weight %1 x 0.1 s").arg(QList<int>({1, 2, 4}).at(s - 56));
    return "Unused (always 0)";
}

bool AHRWWVTimeCodeStrip::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip)
    {
        QHelpEvent *he = static_cast<QHelpEvent*>(e);
        int s = cellAt(he->pos());

        if (s >= 0)
        {
            QString txt = QString("Second %1: %2").arg(s).arg(describeSecond(s));

            if (m_have[s]) {
                txt += QString("\nReceived '%1', confidence %2").arg(QChar(ahrwwv::symbolChar(m_frame[s].symbol)))
                    .arg(m_frame[s].confidence, 0, 'f', 2);
            }

            QToolTip::showText(he->globalPos(), txt, this);
        }
        else
        {
            QToolTip::hideText();
        }

        return true;
    }

    return QWidget::event(e);
}

void AHRWWVTimeCodeStrip::paintEvent(QPaintEvent *event)
{
    (void) event;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    QFont f = font();
    f.setFamily("Liberation Mono");
    f.setBold(true);
    p.setFont(f);

    for (int r = 0; r < 6; r++)
    {
        QRect c0 = cellRect(r * 10);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRect(0, c0.top(), 20, c0.height()), Qt::AlignRight | Qt::AlignVCenter, QString("%1").arg(r * 10, 2, 10, QChar('0')));
    }

    for (int s = 0; s < 60; s++)
    {
        QRect rc = cellRect(s);
        QColor bg = fieldColor(s);

        if (!m_have[s]) {
            bg = bg.darker(220);
        }

        p.fillRect(rc, bg);
        QString txt;
        QColor fg(235, 235, 235);

        if (m_have[s])
        {
            switch (m_frame[s].symbol)
            {
            case Symbol::None: txt = QString::fromUtf8("·"); break;
            case Symbol::Zero: txt = "0"; break;
            case Symbol::One: txt = "1"; fg = QColor(255, 230, 120); break;
            case Symbol::Marker: txt = "M"; fg = QColor(210, 170, 255); break;
            default: txt = "?"; fg = QColor(255, 90, 90); break;
            }

            if (m_frame[s].symbol != Symbol::Unknown && m_frame[s].confidence < 0.5) {
                fg = fg.darker(160); // weak decision
            }
        }

        p.setPen(fg);
        p.drawText(rc, Qt::AlignCenter, txt);

        if (s == m_current)
        {
            p.setPen(QPen(QColor(255, 255, 255), 2));
            p.drawRect(rc.adjusted(0, 0, -1, -1));
        }
    }
}

// ---------------------------------------------------------------- envelope plot

AHRWWVEnvelopePlot::AHRWWVEnvelopePlot(QWidget *parent) :
    QWidget(parent),
    m_rate(100.0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize AHRWWVEnvelopePlot::sizeHint() const { return QSize(360, 150); }
QSize AHRWWVEnvelopePlot::minimumSizeHint() const { return QSize(200, 110); }

void AHRWWVEnvelopePlot::addSamples(const std::vector<float>& samples, double rate)
{
    m_rate = rate;

    for (float v : samples) {
        m_samples.push_back(v);
    }

    size_t maxN = (size_t) (m_spanSeconds * m_rate);

    while (m_samples.size() > maxN) {
        m_samples.pop_front();
    }

    update();
}

void AHRWWVEnvelopePlot::addSecond(const ahrwwv::SecondSymbol& s)
{
    m_seconds.push_back(s);

    while (m_seconds.size() > 20) {
        m_seconds.pop_front();
    }
}

void AHRWWVEnvelopePlot::clear()
{
    m_samples.clear();
    m_seconds.clear();
    update();
}

void AHRWWVEnvelopePlot::paintEvent(QPaintEvent *event)
{
    (void) event;
    QPainter p(this);
    p.fillRect(rect(), QColor(16, 20, 24));

    const int barsW = 110;                 // right-hand panel: window levels of recent seconds
    QRect plot(30, 6, width() - 36 - barsW, height() - 22);
    const double ymin = -0.3, ymax = 1.5;
    auto ypix = [&](double v) {
        v = std::max(ymin, std::min(ymax, v));
        return plot.bottom() - (v - ymin) / (ymax - ymin) * plot.height();
    };

    QFont f = font();
    f.setPointSize(7);
    p.setFont(f);

    // grid: 0 and 1 (pulse) solid, 0.5 decision dashed
    for (double g : {0.0, 0.5, 1.0})
    {
        p.setPen(QPen(QColor(70, 80, 90), 1, g == 0.5 ? Qt::DashLine : Qt::SolidLine));
        p.drawLine(QPointF(plot.left(), ypix(g)), QPointF(plot.right(), ypix(g)));
        p.setPen(QColor(150, 160, 170));
        p.drawText(QRectF(0, ypix(g) - 7, 27, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(g, 'f', 1));
    }

    p.setPen(QColor(150, 160, 170));
    p.drawText(QRect(plot.left(), plot.bottom() + 2, plot.width(), 14), Qt::AlignLeft,
               QString("100 Hz level, last %1 s (pulse = 1.0)").arg(m_spanSeconds));

    if (m_samples.size() > 1)
    {
        size_t maxN = (size_t) (m_spanSeconds * m_rate);
        double dx = plot.width() / (double) std::max<size_t>(maxN - 1, 1);
        double x0 = plot.right() - (m_samples.size() - 1) * dx;
        QPolygonF poly;

        for (size_t i = 0; i < m_samples.size(); i++) {
            poly << QPointF(x0 + i * dx, ypix(m_samples[i]));
        }

        p.setPen(QPen(QColor(80, 220, 160), 1.2));
        p.drawPolyline(poly);
    }

    // window levels A (60-180 ms), B (240-480 ms), C (540-780 ms) of the last seconds
    QRect bars(plot.right() + 8, plot.top(), barsW - 10, plot.height());
    p.setPen(QColor(150, 160, 170));
    p.drawText(QRect(bars.left(), plot.bottom() + 2, bars.width(), 14), Qt::AlignLeft, "A B C per second");
    int n = std::min<int>((int) m_seconds.size(), 8);
    int bw = n > 0 ? bars.width() / 8 : 0;

    for (int k = 0; k < n; k++)
    {
        const ahrwwv::SecondSymbol& s = m_seconds[m_seconds.size() - n + k];
        int x = bars.left() + k * bw;

        for (int i = 0; i < 3; i++)
        {
            double v = std::max(0.0, std::min(1.4, s.levels[i]));
            double h = v / 1.4 * (bars.height() - 12);
            QRectF br(x + i * (bw / 3.0), bars.bottom() - h, bw / 3.0 - 1, h);
            p.fillRect(br, i == 0 ? QColor(90, 160, 220) : i == 1 ? QColor(220, 190, 80) : QColor(190, 120, 230));
        }

        p.setPen(QColor(230, 230, 230));
        p.drawText(QRect(x, bars.top(), bw, 12), Qt::AlignCenter, QString(QChar(ahrwwv::symbolChar(s.symbol))));
    }
}
