// Copyright (c) 2024-2026 The Mynta Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYNTA_COMPAT_GCC15_QT5_COMPAT_H
#define MYNTA_COMPAT_GCC15_QT5_COMPAT_H

/**
 * GCC 15 / Qt 5.15 Compatibility Header
 * 
 * GCC 15 has stricter constexpr checking that exposes a bug in Qt 5.15's
 * qmargins.h where `return *this = ...` is used in constexpr member functions.
 * This is not valid constexpr code since `this` is not a constant expression.
 * 
 * This header must be included BEFORE any Qt headers when using GCC 15+.
 * It redefines Q_DECL_RELAXED_CONSTEXPR to be non-constexpr, which is the
 * correct behavior for these functions.
 */

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 15
// For GCC 15+, override Qt's Q_DECL_RELAXED_CONSTEXPR before Qt headers define it
// We need to prevent Qt from using constexpr for relaxed constexpr functions
// because GCC 15 correctly rejects code that Qt 5.15 incorrectly marked as constexpr
#define Q_DECL_RELAXED_CONSTEXPR
#define MYNTA_GCC15_QT5_WORKAROUND_APPLIED
#endif

#endif // MYNTA_COMPAT_GCC15_QT5_COMPAT_H
