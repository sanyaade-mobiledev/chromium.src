// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/text_button.h"

#include <algorithm>

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus_border.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "skia/ext/skia_utils_win.h"
#include "ui/base/native_theme/native_theme_win.h"
#include "ui/gfx/platform_font_win.h"
#endif

namespace views {

namespace {

// Default space between the icon and text.
const int kDefaultIconTextSpacing = 5;

// Preferred padding between text and edge.
const int kPreferredPaddingHorizontal = 6;
const int kPreferredPaddingVertical = 5;

// Preferred padding between text and edge for NativeTheme border.
const int kPreferredNativeThemePaddingHorizontal = 12;
const int kPreferredNativeThemePaddingVertical = 5;

// By default the focus rect is drawn at the border of the view.
// For a button, we inset the focus rect by 3 pixels so that it
// doesn't draw on top of the button's border. This roughly matches
// how the Windows native focus rect for buttons looks. A subclass
// that draws a button with different padding may need to
// override OnPaintFocusBorder and do something different.
const int kFocusRectInset = 3;

// How long the hover fade animation should last.
const int kHoverAnimationDurationMs = 170;

#if defined(OS_WIN)
// These sizes are from http://msdn.microsoft.com/en-us/library/aa511279.aspx
const int kMinWidthDLUs = 50;
const int kMinHeightDLUs = 14;
#endif

int PrefixTypeToCanvasType(TextButton::PrefixType type) {
  switch (type) {
    case TextButton::PREFIX_HIDE:
      return gfx::Canvas::HIDE_PREFIX;
    case TextButton::PREFIX_SHOW:
      return gfx::Canvas::SHOW_PREFIX;
    case TextButton::PREFIX_NONE:
      return 0;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

// static
const char TextButtonBase::kViewClassName[] = "views/TextButtonBase";
// static
const char TextButton::kViewClassName[] = "views/TextButton";
// static
const char NativeTextButton::kViewClassName[] = "views/NativeTextButton";

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder - constructors, destructors, initialization
//
////////////////////////////////////////////////////////////////////////////////

TextButtonBorder::TextButtonBorder()
    : vertical_padding_(kPreferredPaddingVertical) {
  set_hot_set(BorderImages(BorderImages::kHot));
  set_pushed_set(BorderImages(BorderImages::kPushed));
}

TextButtonBorder::~TextButtonBorder() {
}


////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder - painting
//
////////////////////////////////////////////////////////////////////////////////
void TextButtonBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  const TextButton* button = static_cast<const TextButton*>(&view);
  int state = button->state();

  const BorderImages* set = &normal_set_;
  if (button->show_multiple_icon_states() &&
      ((state == TextButton::BS_HOT) || (state == TextButton::BS_PUSHED)))
    set = (state == TextButton::BS_HOT) ? &hot_set_ : &pushed_set_;
  if (!set->top_left.isNull()) {
    if (button->GetAnimation()->is_animating()) {
      // TODO(pkasting): Really this should crossfade between states so it could
      // handle the case of having a non-NULL |normal_set_|.
      canvas->SaveLayerAlpha(static_cast<uint8>(
          button->GetAnimation()->CurrentValueBetween(0, 255)));
      set->Paint(view.GetLocalBounds(), canvas);
      canvas->Restore();
    } else {
      set->Paint(view.GetLocalBounds(), canvas);
    }
  }
}

void TextButtonBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(vertical_padding_, kPreferredPaddingHorizontal,
              vertical_padding_, kPreferredPaddingHorizontal);
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonNativeThemeBorder
//
////////////////////////////////////////////////////////////////////////////////

TextButtonNativeThemeBorder::TextButtonNativeThemeBorder(
    NativeThemeDelegate* delegate)
    : delegate_(delegate) {
}

TextButtonNativeThemeBorder::~TextButtonNativeThemeBorder() {
}

void TextButtonNativeThemeBorder::Paint(const View& view,
                                        gfx::Canvas* canvas) const {
  const TextButtonBase* tb = static_cast<const TextButton*>(&view);
  const ui::NativeTheme* native_theme = ui::NativeTheme::instance();
  ui::NativeTheme::Part part = delegate_->GetThemePart();
  gfx::Rect rect(delegate_->GetThemePaintRect());

  if (tb->show_multiple_icon_states() &&
      delegate_->GetThemeAnimation() != NULL &&
      delegate_->GetThemeAnimation()->is_animating()) {
    // Paint background state.
    ui::NativeTheme::ExtraParams prev_extra;
    ui::NativeTheme::State prev_state =
        delegate_->GetBackgroundThemeState(&prev_extra);
    native_theme->Paint(canvas->sk_canvas(), part, prev_state, rect,
                        prev_extra);

    // Composite foreground state above it.
    ui::NativeTheme::ExtraParams extra;
    ui::NativeTheme::State state = delegate_->GetForegroundThemeState(&extra);
    int alpha = delegate_->GetThemeAnimation()->CurrentValueBetween(0, 255);
    canvas->SaveLayerAlpha(static_cast<uint8>(alpha));
    native_theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
    canvas->Restore();
  } else {
    ui::NativeTheme::ExtraParams extra;
    ui::NativeTheme::State state = delegate_->GetThemeState(&extra);
    native_theme->Paint(canvas->sk_canvas(), part, state, rect, extra);
  }
}

void TextButtonNativeThemeBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kPreferredNativeThemePaddingVertical,
              kPreferredNativeThemePaddingHorizontal,
              kPreferredNativeThemePaddingVertical,
              kPreferredNativeThemePaddingHorizontal);
}


////////////////////////////////////////////////////////////////////////////////
// TextButtonBase, public:

TextButtonBase::TextButtonBase(ButtonListener* listener, const string16& text)
    : CustomButton(listener),
      alignment_(ALIGN_LEFT),
      font_(ResourceBundle::GetSharedInstance().GetFont(
          ResourceBundle::BaseFont)),
      color_(ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_TextButtonEnabledColor)),
      color_enabled_(ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_TextButtonEnabledColor)),
      color_disabled_(ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_TextButtonDisabledColor)),
      color_highlight_(ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_TextButtonHighlightColor)),
      color_hover_(ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_TextButtonHoverColor)),
      has_text_shadow_(false),
      active_text_shadow_color_(0),
      inactive_text_shadow_color_(0),
      text_shadow_offset_(gfx::Point(1, 1)),
      min_width_(0),
      min_height_(0),
      max_width_(0),
      show_multiple_icon_states_(true),
      is_default_(false),
      multi_line_(false),
      prefix_type_(PREFIX_NONE) {
  SetText(text);
  SetAnimationDuration(kHoverAnimationDurationMs);
}

TextButtonBase::~TextButtonBase() {
}

void TextButtonBase::SetIsDefault(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;
  if (is_default_)
    AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  else
    RemoveAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  SchedulePaint();
}

void TextButtonBase::SetText(const string16& text) {
  if (text == text_)
    return;
  text_ = text;
  SetAccessibleName(text);
  UpdateTextSize();
}

void TextButtonBase::SetFont(const gfx::Font& font) {
  font_ = font;
  UpdateTextSize();
}

void TextButtonBase::SetEnabledColor(SkColor color) {
  color_enabled_ = color;
  UpdateColor();
}

void TextButtonBase::SetDisabledColor(SkColor color) {
  color_disabled_ = color;
  UpdateColor();
}

void TextButtonBase::SetHighlightColor(SkColor color) {
  color_highlight_ = color;
}

void TextButtonBase::SetHoverColor(SkColor color) {
  color_hover_ = color;
}

void TextButtonBase::SetTextShadowColors(SkColor active_color,
                                         SkColor inactive_color) {
  active_text_shadow_color_ = active_color;
  inactive_text_shadow_color_ = inactive_color;
  has_text_shadow_ = true;
}

void TextButtonBase::SetTextShadowOffset(int x, int y) {
  text_shadow_offset_.SetPoint(x, y);
}

void TextButtonBase::ClearEmbellishing() {
  has_text_shadow_ = false;
}

void TextButtonBase::ClearMaxTextSize() {
  max_text_size_ = text_size_;
}

void TextButtonBase::SetShowMultipleIconStates(bool show_multiple_icon_states) {
  show_multiple_icon_states_ = show_multiple_icon_states;
}

void TextButtonBase::SetMultiLine(bool multi_line) {
  if (multi_line != multi_line_) {
    multi_line_ = multi_line;
    max_text_size_.SetSize(0, 0);
    UpdateTextSize();
    SchedulePaint();
  }
}

gfx::Size TextButtonBase::GetPreferredSize() {
  gfx::Insets insets = GetInsets();

  // Use the max size to set the button boundaries.
  // In multiline mode max size can be undefined while
  // width() is 0, so max it out with current text size.
  gfx::Size prefsize(std::max(max_text_size_.width(),
                              text_size_.width()) + insets.width(),
                     std::max(max_text_size_.height(),
                              text_size_.height()) + insets.height());

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

  prefsize.set_width(std::max(prefsize.width(), min_width_));
  prefsize.set_height(std::max(prefsize.height(), min_height_));

  return prefsize;
}

int TextButtonBase::GetHeightForWidth(int w) {
  if (!multi_line_)
    return View::GetHeightForWidth(w);

  if (max_width_ > 0)
    w = std::min(max_width_, w);

  gfx::Size text_size;
  CalculateTextSize(&text_size, w);
  int height = text_size.height() + GetInsets().height();

  return std::max(height, min_height_);
}

void TextButtonBase::OnPaint(gfx::Canvas* canvas) {
  PaintButton(canvas, PB_NORMAL);
}

void TextButtonBase::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (multi_line_)
    UpdateTextSize();
}

const ui::Animation* TextButtonBase::GetAnimation() const {
  return hover_animation_.get();
}

void TextButtonBase::UpdateColor() {
  color_ = enabled() ? color_enabled_ : color_disabled_;
}

void TextButtonBase::UpdateTextSize() {
  int text_width = width();
  // If width is defined, use GetTextBounds.width() for maximum text width,
  // as it will take size of checkbox/radiobutton into account.
  if (text_width != 0) {
    gfx::Rect text_bounds = GetTextBounds();
    text_width = text_bounds.width();
  }
  CalculateTextSize(&text_size_, text_width);
  // Before layout width() is 0, and multiline text will be treated as one line.
  // Do not store max_text_size in this case. UpdateTextSize will be called
  // again once width() changes.
  if (!multi_line_ || text_width != 0) {
    max_text_size_.SetSize(std::max(max_text_size_.width(), text_size_.width()),
                           std::max(max_text_size_.height(),
                                    text_size_.height()));
    PreferredSizeChanged();
  }
}

void TextButtonBase::CalculateTextSize(gfx::Size* text_size, int max_width) {
  int h = font_.GetHeight();
  int w = multi_line_ ? max_width : 0;
  int flags = ComputeCanvasStringFlags();
  if (!multi_line_)
    flags |= gfx::Canvas::NO_ELLIPSIS;

  gfx::Canvas::SizeStringInt(text_, font_, &w, &h, flags);
  text_size->SetSize(w, h);
}

int TextButtonBase::ComputeCanvasStringFlags() const {
  int flags = PrefixTypeToCanvasType(prefix_type_);

  if (multi_line_) {
    flags |= gfx::Canvas::MULTI_LINE;

    switch (alignment_) {
      case ALIGN_LEFT:
        flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
        break;
      case ALIGN_RIGHT:
        flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
        break;
      case ALIGN_CENTER:
        flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
        break;
    }
  }

  return flags;
}

void TextButtonBase::GetExtraParams(
    ui::NativeTheme::ExtraParams* params) const {
  params->button.checked = false;
  params->button.indeterminate = false;
  params->button.is_default = false;
  params->button.is_focused = false;
  params->button.has_border = false;
  params->button.classic_state = 0;
  params->button.background_color =
      ui::NativeTheme::instance()->GetSystemColor(
          ui::NativeTheme::kColorId_TextButtonBackgroundColor);
}

gfx::Rect TextButtonBase::GetContentBounds(int extra_width) const {
  gfx::Insets insets = GetInsets();
  int available_width = width() - insets.width();
  int content_width = text_size_.width() + extra_width;
  int content_x = 0;
  switch(alignment_) {
    case ALIGN_LEFT:
      content_x = insets.left();
      break;
    case ALIGN_RIGHT:
      content_x = width() - insets.right() - content_width;
      if (content_x < insets.left())
        content_x = insets.left();
      break;
    case ALIGN_CENTER:
      content_x = insets.left() + std::max(0,
          (available_width - content_width) / 2);
      break;
  }
  content_width = std::min(content_width,
                           width() - insets.right() - content_x);
  int available_height = height() - insets.height();
  int content_y = (available_height - text_size_.height()) / 2 + insets.top();

  gfx::Rect bounds(content_x, content_y, content_width, text_size_.height());
  return bounds;
}

gfx::Rect TextButtonBase::GetTextBounds() const {
  return GetContentBounds(0);
}

void TextButtonBase::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  if (mode == PB_NORMAL) {
    OnPaintBackground(canvas);
    OnPaintBorder(canvas);
    OnPaintFocusBorder(canvas);
  }

  gfx::Rect text_bounds(GetTextBounds());
  if (text_bounds.width() > 0) {
    // Because the text button can (at times) draw multiple elements on the
    // canvas, we can not mirror the button by simply flipping the canvas as
    // doing this will mirror the text itself. Flipping the canvas will also
    // make the icons look wrong because icons are almost always represented as
    // direction-insensitive images and such images should never be flipped
    // horizontally.
    //
    // Due to the above, we must perform the flipping manually for RTL UIs.
    text_bounds.set_x(GetMirroredXForRect(text_bounds));

    SkColor text_color = (show_multiple_icon_states_ &&
        (state() == BS_HOT || state() == BS_PUSHED)) ? color_hover_ : color_;

    int draw_string_flags = gfx::Canvas::DefaultCanvasTextAlignment() |
        ComputeCanvasStringFlags();

    if (mode == PB_FOR_DRAG) {
      // Disable sub-pixel rendering as background is transparent.
      draw_string_flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;
    }

    if (draw_string_flags & gfx::Canvas::NO_SUBPIXEL_RENDERING) {
#if defined(OS_WIN)
      // TODO(erg): Either port DrawStringWithHalo to linux or find an
      // alternative here.
      canvas->DrawStringWithHalo(text_, font_, text_color, color_highlight_,
          text_bounds.x(), text_bounds.y(), text_bounds.width(),
          text_bounds.height(), draw_string_flags);
#else
      canvas->DrawStringInt(text_,
                            font_,
                            text_color,
                            text_bounds.x(),
                            text_bounds.y(),
                            text_bounds.width(),
                            text_bounds.height(),
                            draw_string_flags);
#endif
    } else {
      gfx::ShadowValues shadows;
      if (has_text_shadow_) {
        SkColor color = GetWidget()->IsActive() ? active_text_shadow_color_ :
                                                  inactive_text_shadow_color_;
        shadows.push_back(gfx::ShadowValue(text_shadow_offset_, 0, color));
      }
      canvas->DrawStringWithShadows(text_, font_, text_color, text_bounds,
                                    draw_string_flags, shadows);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// TextButtonBase, View overrides:

gfx::Size TextButtonBase::GetMinimumSize() {
  return max_text_size_;
}

void TextButtonBase::OnEnabledChanged() {
  // We should always call UpdateColor() since the state of the button might be
  // changed by other functions like CustomButton::SetState().
  UpdateColor();
  CustomButton::OnEnabledChanged();
}

std::string TextButtonBase::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// TextButtonBase, NativeThemeDelegate overrides:

gfx::Rect TextButtonBase::GetThemePaintRect() const {
  return GetLocalBounds();
}

ui::NativeTheme::State TextButtonBase::GetThemeState(
    ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  switch(state()) {
    case BS_DISABLED:
      return ui::NativeTheme::kDisabled;
    case BS_NORMAL:
      return ui::NativeTheme::kNormal;
    case BS_HOT:
      return ui::NativeTheme::kHovered;
    case BS_PUSHED:
      return ui::NativeTheme::kPressed;
    default:
      NOTREACHED() << "Unknown state: " << state();
      return ui::NativeTheme::kNormal;
  }
}

const ui::Animation* TextButtonBase::GetThemeAnimation() const {
#if defined(USE_AURA)
  return hover_animation_.get();
#elif defined(OS_WIN)
  return ui::NativeThemeWin::instance()->IsThemingActive()
      ? hover_animation_.get() : NULL;
#else
  return hover_animation_.get();
#endif
}

ui::NativeTheme::State TextButtonBase::GetBackgroundThemeState(
  ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kNormal;
}

ui::NativeTheme::State TextButtonBase::GetForegroundThemeState(
  ui::NativeTheme::ExtraParams* params) const {
  GetExtraParams(params);
  return ui::NativeTheme::kHovered;
}

////////////////////////////////////////////////////////////////////////////////
//
// TextButton
//
////////////////////////////////////////////////////////////////////////////////

TextButton::TextButton(ButtonListener* listener, const string16& text)
    : TextButtonBase(listener, text),
      icon_placement_(ICON_ON_LEFT),
      has_hover_icon_(false),
      has_pushed_icon_(false),
      icon_text_spacing_(kDefaultIconTextSpacing),
      ignore_minimum_size_(true) {
  set_border(new TextButtonBorder);
  set_focus_border(FocusBorder::CreateDashedFocusBorder(kFocusRectInset,
                                                        kFocusRectInset,
                                                        kFocusRectInset,
                                                        kFocusRectInset));
}

TextButton::~TextButton() {
}

void TextButton::SetIcon(const gfx::ImageSkia& icon) {
  icon_ = icon;
  SchedulePaint();
}

void TextButton::SetHoverIcon(const gfx::ImageSkia& icon) {
  icon_hover_ = icon;
  has_hover_icon_ = true;
  SchedulePaint();
}

void TextButton::SetPushedIcon(const gfx::ImageSkia& icon) {
  icon_pushed_ = icon;
  has_pushed_icon_ = true;
  SchedulePaint();
}

gfx::Size TextButton::GetPreferredSize() {
  gfx::Size prefsize(TextButtonBase::GetPreferredSize());
  prefsize.Enlarge(icon_.width(), 0);
  prefsize.set_height(std::max(prefsize.height(), icon_.height()));

  // Use the max size to set the button boundaries.
  if (icon_.width() > 0 && !text_.empty())
    prefsize.Enlarge(icon_text_spacing_, 0);

  if (max_width_ > 0)
    prefsize.set_width(std::min(max_width_, prefsize.width()));

#if defined(OS_WIN)
  // Clamp the size returned to at least the minimum size.
  if (!ignore_minimum_size_) {
    gfx::PlatformFontWin* platform_font =
        static_cast<gfx::PlatformFontWin*>(font_.platform_font());
    prefsize.set_width(std::max(
        prefsize.width(),
        platform_font->horizontal_dlus_to_pixels(kMinWidthDLUs)));
    prefsize.set_height(std::max(
        prefsize.height(),
        platform_font->vertical_dlus_to_pixels(kMinHeightDLUs)));
  }
#endif

  prefsize.set_width(std::max(prefsize.width(), min_width_));
  prefsize.set_height(std::max(prefsize.height(), min_height_));

  return prefsize;
}

void TextButton::PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) {
  TextButtonBase::PaintButton(canvas, mode);

  const gfx::ImageSkia& icon = GetImageToPaint();

  if (icon.width() > 0) {
    gfx::Rect text_bounds = GetTextBounds();
    int icon_x;
    int spacing = text_.empty() ? 0 : icon_text_spacing_;
    gfx::Insets insets = GetInsets();
    if (icon_placement_ == ICON_ON_LEFT) {
      icon_x = text_bounds.x() - icon.width() - spacing;
    } else if (icon_placement_ == ICON_ON_RIGHT) {
      icon_x = text_bounds.right() + spacing;
    } else {  // ICON_CENTERED
      DCHECK(text_.empty());
      icon_x = (width() - insets.width() - icon.width()) / 2 + insets.left();
    }

    int available_height = height() - insets.height();
    int icon_y = (available_height - icon.height()) / 2 + insets.top();

    // Mirroring the icon position if necessary.
    gfx::Rect icon_bounds(icon_x, icon_y, icon.width(), icon.height());
    icon_bounds.set_x(GetMirroredXForRect(icon_bounds));
    canvas->DrawImageInt(icon, icon_bounds.x(), icon_bounds.y());
  }
}

void TextButton::set_ignore_minimum_size(bool ignore_minimum_size) {
  ignore_minimum_size_ = ignore_minimum_size;
}

std::string TextButton::GetClassName() const {
  return kViewClassName;
}

ui::NativeTheme::Part TextButton::GetThemePart() const {
  return ui::NativeTheme::kPushButton;
}

void TextButton::GetExtraParams(ui::NativeTheme::ExtraParams* params) const {
  TextButtonBase::GetExtraParams(params);
  params->button.is_default = is_default_;
}

gfx::Rect TextButton::GetTextBounds() const {
  int extra_width = 0;

  const gfx::ImageSkia& icon = GetImageToPaint();
  if (icon.width() > 0)
    extra_width = icon.width() + (text_.empty() ? 0 : icon_text_spacing_);

  gfx::Rect bounds(GetContentBounds(extra_width));

  if (extra_width > 0) {
    // Make sure the icon is always fully visible.
    if (icon_placement_ == ICON_ON_LEFT) {
      bounds.Inset(extra_width, 0, 0, 0);
    } else if (icon_placement_ == ICON_ON_RIGHT) {
      bounds.Inset(0, 0, extra_width, 0);
    }
  }

  return bounds;
}

const gfx::ImageSkia& TextButton::GetImageToPaint() const {
  if (show_multiple_icon_states_) {
    if (has_hover_icon_ && (state() == BS_HOT))
      return icon_hover_;
    if (has_pushed_icon_ && (state() == BS_PUSHED))
      return icon_pushed_;
  }
  return icon_;
}

////////////////////////////////////////////////////////////////////////////////
//
// NativeTextButton
//
////////////////////////////////////////////////////////////////////////////////

NativeTextButton::NativeTextButton(ButtonListener* listener)
    : TextButton(listener, string16()) {
  Init();
}

NativeTextButton::NativeTextButton(ButtonListener* listener,
                                   const string16& text)
    : TextButton(listener, text) {
  Init();
}

void NativeTextButton::Init() {
#if defined(OS_WIN)
  // Use applicable Windows system colors.
  color_enabled_ = skia::COLORREFToSkColor(GetSysColor(COLOR_BTNTEXT));
  color_disabled_ = skia::COLORREFToSkColor(GetSysColor(COLOR_GRAYTEXT));
  color_hover_ = color_ = color_enabled_;
#endif
  set_border(new TextButtonNativeThemeBorder(this));
#if !defined(OS_WIN)
  // Paint nothing, focus will be indicated with a border highlight drawn by
  // NativeThemeBase::PaintButton.
  set_focus_border(NULL);
#endif
  set_ignore_minimum_size(false);
  set_alignment(ALIGN_CENTER);
  set_focusable(true);
}

gfx::Size NativeTextButton::GetMinimumSize() {
  return GetPreferredSize();
}

std::string NativeTextButton::GetClassName() const {
  return kViewClassName;
}

void NativeTextButton::GetExtraParams(
    ui::NativeTheme::ExtraParams* params) const {
  TextButton::GetExtraParams(params);
  params->button.has_border = true;
#if !defined(OS_WIN)
  // Windows may paint a dotted focus rect in
  // NativeTextButton::OnPaintFocusBorder. To avoid getting two focus
  // indications (A dotted rect and a highlighted border) only set is_focused on
  // non windows platforms.
  params->button.is_focused = HasFocus() &&
      (focusable() || IsAccessibilityFocusable());
#endif
}

}  // namespace views
