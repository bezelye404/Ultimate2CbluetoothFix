// Tells the user in plain words what is missing and how to fix it. It only shows commands to copy: the program
// never runs them (they need administrator rights, and the user stays in control).
#pragma once
#include <QDialog>

#include <string>

#include "u2c/translations.h"
#include "u2c/ui/policy.h"

namespace u2c::uiqt {

class NoticeDialog : public QDialog {
public:
    NoticeDialog(const Catalog& catalog, ui::UiNotice notice, const std::string& detail, ui::Distro distro, QWidget* parent);
};

}  // namespace u2c::uiqt
