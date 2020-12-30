// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/askpassphrasedialog.h>
#include <qt/forms/ui_askpassphrasedialog.h>

#include <qt/guiconstants.h>
#include <qt/walletmodel.h>

#include <support/allocators/secure.h>

#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>

AskPassphraseDialog::AskPassphraseDialog(Mode _mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AskPassphraseDialog),
    mode(_mode),
    model(0),
    fCapsLock(false)
{
    ui->setupUi(this);

    ui->passEdit1->setMinimumSize(ui->passEdit1->sizeHint());
    ui->passEdit2->setMinimumSize(ui->passEdit2->sizeHint());
    ui->passEdit3->setMinimumSize(ui->passEdit3->sizeHint());

    ui->passEdit1->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit2->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit3->setMaxLength(MAX_PASSPHRASE_SIZE);

    // Setup Caps Lock detection.
    ui->passEdit1->installEventFilter(this);
    ui->passEdit2->installEventFilter(this);
    ui->passEdit3->installEventFilter(this);

    switch(mode)
    {
        case Encrypt: // Ask passphrase x2
            ui->warningLabel->setText(tr("Enter the new passphrase to the wallet.<br/>Please use a passphrase of <b>ten or more random characters</b>, or <b>eight or more words</b>."));
            ui->passLabel1->hide();
            ui->passEdit1->hide();
            setWin