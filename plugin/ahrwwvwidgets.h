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

#ifndef INCLUDE_AHRWWVWIDGETS_H
#define INCLUDE_AHRWWVWIDGETS_H

#include <array>
#include <deque>

#include <QWidget>

#include "ahrwwv/decoder.h"

//! 60-second time-code monitor: 6 rows x 10 seconds. Each cell shows the
//! symbol received in that second of the minute (0, 1, M = position marker,
//! ? = unreadable, blank = not yet received), tinted by the field the bit
//! belongs to. The current second is outlined. Hover a cell for its meaning.
class AHRWWVTimeCodeStrip : public QWidget
{
    Q_OBJECT
public:
    explicit AHRWWVTimeCodeStrip(QWidget *parent = nullptr);

    void setFrame(const std::array<ahrwwv::SecondSymbol, 60>& frame, int currentPosition);
    void clear();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    static QString describeSecond(int second); //!< e.g. "minute units, weight 4"

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    std::array<ahrwwv::SecondSymbol, 60> m_frame;
    std::array<bool, 60> m_have;
    int m_current;

    QRect cellRect(int second) const;
    int cellAt(const QPoint& p) const;
    static QColor fieldColor(int second);
};

//! Scrolling plot of the recovered 100 Hz subcarrier level (pulse ~1.0,
//! gap ~0) with the 50 % decision line, and the window levels of the most
//! recent seconds. Purely diagnostic.
class AHRWWVEnvelopePlot : public QWidget
{
    Q_OBJECT
public:
    explicit AHRWWVEnvelopePlot(QWidget *parent = nullptr);

    void addSamples(const std::vector<float>& samples, double rate);
    void addSecond(const ahrwwv::SecondSymbol& s);
    void clear();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::deque<float> m_samples;
    std::deque<ahrwwv::SecondSymbol> m_seconds;
    double m_rate;
    static const int m_spanSeconds = 4;
};

#endif // INCLUDE_AHRWWVWIDGETS_H
