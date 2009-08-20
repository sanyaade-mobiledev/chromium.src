// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/uninstall_view.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/shell_util.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/standard_layout.h"

#include "grit/chromium_strings.h"

UninstallView::UninstallView(int& user_selection)
    : confirm_label_(NULL),
      delete_profile_(NULL),
      change_default_browser_(NULL),
      browsers_combo_(NULL),
      browsers_(NULL),
      user_selection_(user_selection) {
  SetupControls();
}

UninstallView::~UninstallView() {
  // Exit the message loop we were started with so that uninstall can continue.
  MessageLoop::current()->Quit();
}

void UninstallView::SetupControls() {
  using views::ColumnSet;
  using views::GridLayout;

  GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  // Message to confirm uninstallation.
  int column_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, column_set_id);
  confirm_label_ = new views::Label(l10n_util::GetString(IDS_UNINSTALL_VERIFY));
  confirm_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(confirm_label_);

  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  // The "delete profile" check box.
  ++column_set_id;
  column_set = layout->AddColumnSet(column_set_id);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, column_set_id);
  delete_profile_ = new views::Checkbox(
      l10n_util::GetString(IDS_UNINSTALL_DELETE_PROFILE));
  layout->AddView(delete_profile_);

  // Set default browser combo box
  if (ShellIntegration::IsDefaultBrowser()) {
    browsers_.reset(new BrowsersMap());
    ShellUtil::GetRegisteredBrowsers(browsers_.get());
    if (!browsers_->empty()) {
      layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

      ++column_set_id;
      column_set = layout->AddColumnSet(column_set_id);
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
      column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
      column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
      column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
      layout->StartRow(0, column_set_id);
      change_default_browser_ = new views::Checkbox(
          l10n_util::GetString(IDS_UNINSTALL_SET_DEFAULT_BROWSER));
      change_default_browser_->set_listener(this);
      layout->AddView(change_default_browser_);
      browsers_combo_ = new views::Combobox(this);
      layout->AddView(browsers_combo_);
      browsers_combo_->SetEnabled(false);
    }
  }

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
}

bool UninstallView::Accept() {
  user_selection_ = ResultCodes::NORMAL_EXIT;
  if (delete_profile_->checked())
    user_selection_ = ResultCodes::UNINSTALL_DELETE_PROFILE;
  if (change_default_browser_ && change_default_browser_->checked()) {
    int index = browsers_combo_->selected_item();
    BrowsersMap::const_iterator it = browsers_->begin();
    std::advance(it, index);
    base::LaunchApp((*it).second, false, true, NULL);
  }
  return true;
}

bool UninstallView::Cancel() {
  user_selection_ = ResultCodes::UNINSTALL_USER_CANCEL;
  return true;
}

std::wstring UninstallView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  // We only want to give custom name to OK button - 'Uninstall'. Cancel
  // button remains same.
  std::wstring label = L"";
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    label = l10n_util::GetString(IDS_UNINSTALL_BUTTON_TEXT);
  return label;
}

void UninstallView::ButtonPressed(views::Button* sender) {
  if (change_default_browser_ == sender) {
    // Disable the browsers combobox if the user unchecks the checkbox.
    DCHECK(browsers_combo_);
    browsers_combo_->SetEnabled(change_default_browser_->checked());
  }
}

std::wstring UninstallView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_UNINSTALL_CHROME);
}

views::View* UninstallView::GetContentsView() {
  return this;
}

int UninstallView::GetItemCount(views::Combobox* source) {
  DCHECK(source == browsers_combo_);
  DCHECK(!browsers_->empty());
  return browsers_->size();
}

std::wstring UninstallView::GetItemAt(views::Combobox* source, int index) {
  DCHECK(source == browsers_combo_);
  DCHECK(index < (int) browsers_->size());
  BrowsersMap::const_iterator it = browsers_->begin();
  std::advance(it, index);
  return (*it).first;
}

