// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_QT_GUICONSTANTS_H
#define MYNTA_QT_GUICONSTANTS_H

/* Milliseconds between model updates */
static const int MODEL_UPDATE_DELAY = 250;

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* MyntaGUI -- Size of icons in status bar */
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/* Invalid field background style */
#define STYLE_INVALID "background:#FF8080; border: 1px solid #334155; padding: 0px; color: #0f172a;"
#define STYLE_VALID "background: #1e293b; border: 1px solid #334155; padding: 0px; color: white;"

/* Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(128, 128, 128)
/* Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(255, 0, 0)
/* Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(140, 140, 140)
/* Transaction list -- TX status decoration - open until date */
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(64, 64, 255)
/* Transaction list -- TX status decoration - danger, tx needs attention */
#define COLOR_TX_STATUS_DANGER QColor(200, 100, 100)
/* Transaction list -- TX status decoration - default color */
#define COLOR_BLACK QColor(0, 0, 0)
/* Widget Background color - default color */
#define COLOR_WHITE QColor(255, 255, 255)

#define COLOR_WALLETFRAME_SHADOW QColor(0,0,0,71)

/* Color of labels - Gold accent */
#define COLOR_LABELS QColor("#fbbf24")

/** LIGHT MODE - Updated to match myntacoin.org dark website design */
/* Background color - dark-900 (matches website) */
#define COLOR_BACKGROUND_LIGHT QColor("#0f172a")
/* Mynta primary gold */
#define COLOR_DARK_ORANGE QColor("#f59e0b")
/* Mynta light gold */
#define COLOR_LIGHT_ORANGE QColor("#fbbf24")
/* Sidebar dark color - dark-900 */
#define COLOR_DARK_BLUE QColor("#0f172a")
/* Sidebar light color - dark-800 */
#define COLOR_LIGHT_BLUE QColor("#1e293b")
/* Mynta asset text */
#define COLOR_ASSET_TEXT QColor(255, 255, 255)
/* Mynta shadow color - dark-950 */
#define COLOR_SHADOW_LIGHT QColor("#020617")
/* Toolbar not selected text color - dark-400 */
#define COLOR_TOOLBAR_NOT_SELECTED_TEXT QColor("#94a3b8")
/* Toolbar selected text color - Gold */
#define COLOR_TOOLBAR_SELECTED_TEXT QColor("#fbbf24")
/* Send entries background color - dark-800 */
#define COLOR_SENDENTRIES_BACKGROUND QColor("#1e293b")


/** DARK MODE - Official Mynta Brand Colors from myntacoin.org */
/* Widget background color, dark mode - dark-800 */
#define COLOR_WIDGET_BACKGROUND_DARK QColor("#1e293b")
/* Mynta shadow color - dark mode - dark-950 */
#define COLOR_SHADOW_DARK QColor("#020617")
/* Mynta Light background - dark mode - dark-700 */
#define COLOR_LIGHT_BLUE_DARK QColor("#334155")
/* Mynta Dark background - dark mode - dark-900 */
#define COLOR_DARK_BLUE_DARK QColor("#0f172a")
/* Pricing widget background color - dark-800 */
#define COLOR_PRICING_WIDGET QColor("#1e293b")
/* Mynta dark mode administrator background color */
#define COLOR_ADMIN_CARD_DARK COLOR_BLACK
/* Mynta dark mode regular asset background color - dark-900 */
#define COLOR_REGULAR_CARD_DARK_BLUE_DARK_MODE QColor("#0f172a")
/* Mynta dark mode regular asset background color - dark-800 */
#define COLOR_REGULAR_CARD_LIGHT_BLUE_DARK_MODE QColor("#1e293b")
/* Toolbar not selected text color - dark-400 */
#define COLOR_TOOLBAR_NOT_SELECTED_TEXT_DARK_MODE QColor("#94a3b8")
/* Toolbar selected text color - Gold */
#define COLOR_TOOLBAR_SELECTED_TEXT_DARK_MODE QColor("#fbbf24")
/* Send entries background color dark mode - dark-800 */
#define COLOR_SENDENTRIES_BACKGROUND_DARK QColor("#1e293b")


/* Mynta label color as a string - Gold */
#define STRING_LABEL_COLOR "color: #fbbf24"
#define STRING_LABEL_COLOR_WARNING "color: #FF8080"








/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Maximum allowed URI length */
static const int MAX_URI_LENGTH = 255;

/* QRCodeDialog -- size of exported QR Code image */
#define QR_IMAGE_SIZE 300

/* Number of frames in spinner animation */
#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "Mynta"
#define QAPP_ORG_DOMAIN "mynta.network"
#define QAPP_APP_NAME_DEFAULT "Mynta-Qt"
#define QAPP_APP_NAME_TESTNET "Mynta-Qt-testnet"

/* Default third party browser urls - Mynta explorers */
#define DEFAULT_THIRD_PARTY_BROWSERS ""

/* Default IPFS viewer */
#define DEFAULT_IPFS_VIEWER "https://ipfs.io/ipfs/%s"

#endif // MYNTA_QT_GUICONSTANTS_H
