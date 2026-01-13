/*
###############################################################################
#                                                                             #
# The MIT License                                                             #
#                                                                             #
# Copyright (C) 2017 by Juergen Skrotzky (JorgenVikingGod@gmail.com)          #
#               >> https://github.com/Jorgen-VikingGod                        #
#                                                                             #
# Sources: https://github.com/Jorgen-VikingGod/Qt-Frameless-Window-DarkStyle  #
#                                                                             #
###############################################################################
*/

#include <QDebug>
#include "darkstyle.h"

DarkStyle::DarkStyle():
  DarkStyle(styleBase())
{ }

DarkStyle::DarkStyle(QStyle *style):
  QProxyStyle(style)
{ }

QStyle *DarkStyle::styleBase(QStyle *style) const {
  static QStyle *base = !style ? QStyleFactory::create(QStringLiteral("Fusion")) : style;
  return base;
}

QStyle *DarkStyle::baseStyle() const
{
  return styleBase();
}

void DarkStyle::polish(QPalette &palette)
{
  // Mynta brand dark palette with Gold accent
  // dark-900: #0f172a, dark-800: #1e293b, dark-700: #334155
  // gold-400: #fbbf24, gold-500: #f59e0b
  
  palette.setColor(QPalette::Window, QColor(0x1e, 0x29, 0x3b));           // dark-800
  palette.setColor(QPalette::WindowText, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x94, 0xa3, 0xb8)); // dark-400
  palette.setColor(QPalette::Base, QColor(0x0f, 0x17, 0x2a));             // dark-900
  palette.setColor(QPalette::AlternateBase, QColor(0x33, 0x41, 0x55));    // dark-700
  palette.setColor(QPalette::ToolTipBase, QColor(0xfe, 0xf3, 0xc7));      // amber-100
  palette.setColor(QPalette::ToolTipText, QColor(0x0f, 0x17, 0x2a));      // dark-900
  palette.setColor(QPalette::Text, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x64, 0x74, 0x8b));       // dark-500
  palette.setColor(QPalette::Dark, QColor(0x02, 0x06, 0x17));             // dark-950
  palette.setColor(QPalette::Shadow, QColor(0x02, 0x06, 0x17));           // dark-950
  palette.setColor(QPalette::Button, QColor(0x33, 0x41, 0x55));           // dark-700
  palette.setColor(QPalette::ButtonText, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x64, 0x74, 0x8b)); // dark-500
  palette.setColor(QPalette::BrightText, QColor(0xfb, 0xbf, 0x24));       // gold-400
  palette.setColor(QPalette::Link, QColor(0xfb, 0xbf, 0x24));             // gold-400
  palette.setColor(QPalette::Highlight, QColor(0xf5, 0x9e, 0x0b));        // gold-500
  palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x33, 0x41, 0x55));  // dark-700
  palette.setColor(QPalette::HighlightedText, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x64, 0x74, 0x8b)); // dark-500
}

void DarkStyle::polish(QApplication *app)
{
  if (!app) return;

// increase font size for better reading,
// setPointSize was reduced from +2 because when applied this way in Qt5, the font is larger than intended for some reason
//  QFont defaultFont = QApplication::font();
//  defaultFont.setPointSize(defaultFont.pointSize()+1);
//  app->setFont(defaultFont);

  // loadstylesheet
  QFile qfDarkstyle(QStringLiteral(":/darkstyle/qss"));
  if (qfDarkstyle.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    // set stylesheet
    QString qsStylesheet = QString::fromLatin1(qfDarkstyle.readAll());
    app->setStyleSheet(qsStylesheet);
    qfDarkstyle.close();
  }
}
