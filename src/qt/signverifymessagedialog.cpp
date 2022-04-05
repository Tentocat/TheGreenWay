// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/signverifymessagedialog.h>
#include <qt/forms/ui_signverifymessagedialog.h>

#include <qt/addressbookpage.h>
#include <qt/guiutil.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <base58.h>
#include <init.h>
#include <validation.h> // For strMessageMagic
#include <wallet/wallet.h>

#include <string>
#include <vector>

#include <QClipboard>

SignVerifyMessageDialog::SignVerifyMessageDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignVerifyMessageDialog),
    model(0),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    ui->addressBookButton_SM->setIcon(platformStyle->SingleColorIcon(":/icons/address-book"));
    ui->pasteButton_SM->setIcon(platformStyle->SingleColorIcon(":/icons/editpaste"));
    ui->copySignatureButton_SM->setIcon(platformStyle->SingleColorIcon(":/icons/editcopy"));
    ui->signMessageButton_SM->setIcon(platformStyle->SingleColorIcon(":/icons/edit"));
    ui->clearButton_SM->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->addressBookButton_VM->setIcon(platformStyle->SingleColorIcon(":/icons/address-book"));
    ui->verifyMessageButton_VM->setIcon(platformStyle->SingleColorIcon(":/icons/transaction_0"));
    ui->clearButton_VM->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));

#if QT_VERSION >= 0x040700
    ui->signatureOut_SM->setPlaceholderText(tr("Click \"Sign Message\" to generate signature"));
#endif

    GUIUtil::setupAddressWidget(ui->addressIn_SM, this);
    GUIUtil::setupAddressWidget(ui->addressIn_VM, this);

    ui->addressIn_SM->installEventFilter(this);
    ui->messageIn_SM->installEventFilter(this);
    ui->signatureOut_SM->installEventFilter(this);
    ui->addressIn_VM->installEventFilter(this);
    ui->messageIn_VM->installEventFilter(this);
    ui->signatureIn_VM->installEventFilter(this);

    ui->signatureOut_SM->setFont(GUIUtil::fixedPitchFont());
    ui->signatureIn_VM->setFont(GUIUtil::fixedPitchFont());
}

SignVerifyMessageDialog::~SignVerifyMessageDialog()
{
    delete ui;
}

void SignVerifyMessageDialog::setModel(WalletModel *_model)
{
    this->model = _model;
}

void SignVerifyMessageDialog::setAddress_SM(const QString &address)
{
    ui->addressIn_SM->setText(address);
    ui->messageIn_SM->setFocus();
}

void SignVerifyMessageDialog::setAddress_VM(const QString &address)
{
    ui->addressIn_VM->setText(address);
    ui->messageIn_VM->setFocus();
}

void SignVerifyMessageDialog::showTab_SM(bool fShow)
{
    ui->tabWidget->setCurrentIndex(0);
    if (fShow)
        this->show();
}

void SignVerifyMessageDialog::showTab_VM(bool fShow)
{
    ui->tabWidget->setCurrentIndex(1);
    if (fShow)
        this->show();
}

void SignVerifyMessageDialog::on_addressBookButton_SM_clicked()
{
    if (model && model->getAddressTableModel())
    {
        AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec())
        {
            setAddress_SM(dlg.getReturnValue());
        }
    }
}

void SignVerifyMessageDialog::on_pasteButton_SM_clicked()
{
    setAddress_SM(QApplication::clipboard()->text());
}

void SignVerifyMessageDialog::on_signMessageButton_SM_clicked()
{
    if (!model)
        return;

    /* Clear old signature to ensure users don't get confused on error with an old signature displayed */
    ui->signatureOut_SM->clear();

    CTxDestination destination = DecodeDestination(ui->addressIn_SM->text().toStdString());
    if (!IsValidDestination(destination)) {
        ui->statusLabel_SM->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_SM->setText(tr("The entered address is invalid.") + QString(" ") + tr("Please check the address and try again."));
        ret