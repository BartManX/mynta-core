// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_QT_MYNTAADDRESSVALIDATOR_H
#define MYNTA_QT_MYNTAADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class MyntaAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit MyntaAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** Mynta address widget validator, checks for a valid mynta address.
 */
class MyntaAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit MyntaAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // MYNTA_QT_MYNTAADDRESSVALIDATOR_H
