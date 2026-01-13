#pragma once

#include <QObject>
#include <QColor>
#include <QQmlEngine>

namespace phoenix::editor {

/**
 * @brief Application theme - provides colors, fonts, and sizing constants
 * 
 * This is registered as a singleton context property for QML access.
 */
class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // ========== Colors ==========
    
    // Background colors
    Q_PROPERTY(QColor bg1 READ bg1 CONSTANT)
    Q_PROPERTY(QColor bg2 READ bg2 CONSTANT)
    Q_PROPERTY(QColor bg3 READ bg3 CONSTANT)
    Q_PROPERTY(QColor bg4 READ bg4 CONSTANT)
    Q_PROPERTY(QColor bg5 READ bg5 CONSTANT)
    
    // Accent colors
    Q_PROPERTY(QColor accent READ accent CONSTANT)
    Q_PROPERTY(QColor accentHover READ accentHover CONSTANT)
    Q_PROPERTY(QColor accentDim READ accentDim CONSTANT)
    Q_PROPERTY(QColor accentBg READ accentBg CONSTANT)
    
    // Secondary accent
    Q_PROPERTY(QColor secondary READ secondary CONSTANT)
    Q_PROPERTY(QColor secondaryDim READ secondaryDim CONSTANT)
    
    // Text colors
    Q_PROPERTY(QColor textPrimary READ textPrimary CONSTANT)
    Q_PROPERTY(QColor textSecondary READ textSecondary CONSTANT)
    Q_PROPERTY(QColor textDim READ textDim CONSTANT)
    Q_PROPERTY(QColor textAccent READ textAccent CONSTANT)
    
    // Border colors
    Q_PROPERTY(QColor border READ border CONSTANT)
    Q_PROPERTY(QColor borderLight READ borderLight CONSTANT)
    Q_PROPERTY(QColor borderAccent READ borderAccent CONSTANT)
    
    // Track colors
    Q_PROPERTY(QColor videoTrack READ videoTrack CONSTANT)
    Q_PROPERTY(QColor videoTrackLight READ videoTrackLight CONSTANT)
    Q_PROPERTY(QColor audioTrack READ audioTrack CONSTANT)
    Q_PROPERTY(QColor audioTrackLight READ audioTrackLight CONSTANT)
    
    // Clip colors
    Q_PROPERTY(QColor clipVideo READ clipVideo CONSTANT)
    Q_PROPERTY(QColor clipVideoSelected READ clipVideoSelected CONSTANT)
    Q_PROPERTY(QColor clipAudio READ clipAudio CONSTANT)
    Q_PROPERTY(QColor clipAudioSelected READ clipAudioSelected CONSTANT)
    
    // State colors
    Q_PROPERTY(QColor success READ success CONSTANT)
    Q_PROPERTY(QColor warning READ warning CONSTANT)
    Q_PROPERTY(QColor error READ error CONSTANT)
    Q_PROPERTY(QColor info READ info CONSTANT)
    
    // Playhead
    Q_PROPERTY(QColor playhead READ playhead CONSTANT)
    Q_PROPERTY(QColor playheadGlow READ playheadGlow CONSTANT)
    Q_PROPERTY(QColor shadowColor READ shadowColor CONSTANT)
    
    // ========== Typography ==========
    
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(QString monoFont READ monoFont CONSTANT)
    
    Q_PROPERTY(int fontSizeXs READ fontSizeXs CONSTANT)
    Q_PROPERTY(int fontSizeSm READ fontSizeSm CONSTANT)
    Q_PROPERTY(int fontSizeMd READ fontSizeMd CONSTANT)
    Q_PROPERTY(int fontSizeLg READ fontSizeLg CONSTANT)
    Q_PROPERTY(int fontSizeXl READ fontSizeXl CONSTANT)
    Q_PROPERTY(int fontSizeXxl READ fontSizeXxl CONSTANT)
    
    Q_PROPERTY(int fontWeightLight READ fontWeightLight CONSTANT)
    Q_PROPERTY(int fontWeightNormal READ fontWeightNormal CONSTANT)
    Q_PROPERTY(int fontWeightMedium READ fontWeightMedium CONSTANT)
    Q_PROPERTY(int fontWeightBold READ fontWeightBold CONSTANT)
    
    // ========== Spacing ==========
    
    Q_PROPERTY(int spacingXs READ spacingXs CONSTANT)
    Q_PROPERTY(int spacingSm READ spacingSm CONSTANT)
    Q_PROPERTY(int spacingMd READ spacingMd CONSTANT)
    Q_PROPERTY(int spacingLg READ spacingLg CONSTANT)
    Q_PROPERTY(int spacingXl READ spacingXl CONSTANT)
    Q_PROPERTY(int spacingXxl READ spacingXxl CONSTANT)
    
    // ========== Sizing ==========
    
    Q_PROPERTY(int trackHeight READ trackHeight CONSTANT)
    Q_PROPERTY(int trackHeaderWidth READ trackHeaderWidth CONSTANT)
    Q_PROPERTY(int toolbarHeight READ toolbarHeight CONSTANT)
    Q_PROPERTY(int transportHeight READ transportHeight CONSTANT)
    Q_PROPERTY(int timeRulerHeight READ timeRulerHeight CONSTANT)
    Q_PROPERTY(int scrollbarWidth READ scrollbarWidth CONSTANT)
    
    Q_PROPERTY(int iconSizeSm READ iconSizeSm CONSTANT)
    Q_PROPERTY(int iconSizeMd READ iconSizeMd CONSTANT)
    Q_PROPERTY(int iconSizeLg READ iconSizeLg CONSTANT)
    Q_PROPERTY(int iconSizeXl READ iconSizeXl CONSTANT)
    
    // ========== Radius ==========
    
    Q_PROPERTY(int radiusSm READ radiusSm CONSTANT)
    Q_PROPERTY(int radiusMd READ radiusMd CONSTANT)
    Q_PROPERTY(int radiusLg READ radiusLg CONSTANT)
    Q_PROPERTY(int radiusXl READ radiusXl CONSTANT)
    
    // ========== Animation ==========
    
    Q_PROPERTY(int animFast READ animFast CONSTANT)
    Q_PROPERTY(int animNormal READ animNormal CONSTANT)
    Q_PROPERTY(int animSlow READ animSlow CONSTANT)
    
    Q_PROPERTY(qreal shadowOpacity READ shadowOpacity CONSTANT)

public:
    explicit Theme(QObject* parent = nullptr) : QObject(parent) {}
    
    // Colors
    QColor bg1() const { return QColor("#0d0d0d"); }
    QColor bg2() const { return QColor("#141414"); }
    QColor bg3() const { return QColor("#1a1a1a"); }
    QColor bg4() const { return QColor("#222222"); }
    QColor bg5() const { return QColor("#2a2a2a"); }
    
    QColor accent() const { return QColor("#00d4aa"); }
    QColor accentHover() const { return QColor("#00eebb"); }
    QColor accentDim() const { return QColor("#008866"); }
    QColor accentBg() const { return QColor(0, 212, 170, 32); }
    
    QColor secondary() const { return QColor("#ff6b35"); }
    QColor secondaryDim() const { return QColor("#cc5529"); }
    
    QColor textPrimary() const { return QColor("#f0f0f0"); }
    QColor textSecondary() const { return QColor("#a0a0a0"); }
    QColor textDim() const { return QColor("#606060"); }
    QColor textAccent() const { return accent(); }
    
    QColor border() const { return QColor("#333333"); }
    QColor borderLight() const { return QColor("#404040"); }
    QColor borderAccent() const { return accent(); }
    
    QColor videoTrack() const { return QColor("#1e3a5f"); }
    QColor videoTrackLight() const { return QColor("#2a5080"); }
    QColor audioTrack() const { return QColor("#3d1e5f"); }
    QColor audioTrackLight() const { return QColor("#502a80"); }
    
    QColor clipVideo() const { return QColor("#2196F3"); }
    QColor clipVideoSelected() const { return QColor("#64B5F6"); }
    QColor clipAudio() const { return QColor("#9C27B0"); }
    QColor clipAudioSelected() const { return QColor("#BA68C8"); }
    
    QColor success() const { return QColor("#4CAF50"); }
    QColor warning() const { return QColor("#FFC107"); }
    QColor error() const { return QColor("#f44336"); }
    QColor info() const { return QColor("#2196F3"); }
    
    QColor playhead() const { return QColor("#ff3366"); }
    QColor playheadGlow() const { return QColor(255, 51, 102, 64); }
    QColor shadowColor() const { return QColor("#000000"); }
    
    // Typography
    QString fontFamily() const { return "Segoe UI, SF Pro Display, -apple-system, sans-serif"; }
    QString monoFont() const { return "JetBrains Mono, Consolas, monospace"; }
    
    int fontSizeXs() const { return 10; }
    int fontSizeSm() const { return 11; }
    int fontSizeMd() const { return 13; }
    int fontSizeLg() const { return 15; }
    int fontSizeXl() const { return 18; }
    int fontSizeXxl() const { return 24; }
    
    int fontWeightLight() const { return 300; }
    int fontWeightNormal() const { return 400; }
    int fontWeightMedium() const { return 500; }
    int fontWeightBold() const { return 600; }
    
    // Spacing
    int spacingXs() const { return 4; }
    int spacingSm() const { return 8; }
    int spacingMd() const { return 12; }
    int spacingLg() const { return 16; }
    int spacingXl() const { return 24; }
    int spacingXxl() const { return 32; }
    
    // Sizing
    int trackHeight() const { return 60; }
    int trackHeaderWidth() const { return 180; }
    int toolbarHeight() const { return 48; }
    int transportHeight() const { return 64; }
    int timeRulerHeight() const { return 28; }
    int scrollbarWidth() const { return 10; }
    
    int iconSizeSm() const { return 16; }
    int iconSizeMd() const { return 20; }
    int iconSizeLg() const { return 24; }
    int iconSizeXl() const { return 32; }
    
    // Radius
    int radiusSm() const { return 4; }
    int radiusMd() const { return 6; }
    int radiusLg() const { return 8; }
    int radiusXl() const { return 12; }
    
    // Animation
    int animFast() const { return 100; }
    int animNormal() const { return 200; }
    int animSlow() const { return 300; }
    
    qreal shadowOpacity() const { return 0.4; }
};

} // namespace phoenix::editor
