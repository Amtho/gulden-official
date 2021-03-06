// Copyright (c) 2016 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "nockssettings.h"
#include "utilmoneystr.h"
#include "arith_uint256.h"
#include "bitcoinunits.h"

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QVariant>
#include <QModelIndex>

#include <ostream>
#include <iomanip>
#include <sstream>

#include <openssl/x509_vfy.h>

NocksSettings::NocksSettings(QObject* parent)
    : optionsModel(NULL)
{
    netManager = new QNetworkAccessManager(this);

    connect(netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(netRequestFinished(QNetworkReply*)));
    connect(netManager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)), this, SLOT(reportSslErrors(QNetworkReply*, const QList<QSslError>&)));

    exchangeLimits["BTC-NLG"] = std::make_pair("250.00000", "50000.00000");
    exchangeLimits["NLG-BTC"] = std::make_pair("0.00060", "1.00000");
    exchangeLimits["NLG-EUR"] = std::make_pair("0.01", "500.00");
    exchangeLimits["EUR-NLG"] = std::make_pair("250.00000", "50000.00000");
}

NocksSettings::~NocksSettings()
{
}

void NocksSettings::setOptionsModel(OptionsModel* optionsModel_)
{
    /*optionsModel = optionsModel;

    if ( optionsModel )
    {
        QNetworkProxy proxy;


        if ( optionsModel->getProxySettings( proxy ) )
        {
            netManager->setProxy( proxy );
            qDebug() << "PaymentServer::initNetManager: Using SOCKS5 proxy" << proxy.hostName() << ":" << proxy.port();
        }
        else
            qDebug() << "PaymentServer::initNetManager: No active proxy server found.";
    }*/
}

CAmount NocksSettings::getMinimumForCurrency(std::string symbol)
{
    if (exchangeLimits.count(symbol) == 0)
        return 0;

    CAmount amountMinimum;
    BitcoinUnits::parse(BitcoinUnits::BTC, QString::fromStdString(exchangeLimits[symbol].first), &amountMinimum);
    return amountMinimum;
}

CAmount NocksSettings::getMaximumForCurrency(std::string symbol)
{
    if (exchangeLimits.count(symbol) == 0)
        return 0;

    CAmount amountMaximum;
    BitcoinUnits::parse(BitcoinUnits::BTC, QString::fromStdString(exchangeLimits[symbol].second), &amountMaximum);
    return amountMaximum;
}

std::string NocksSettings::getMinimumForCurrencyAsString(std::string symbol)
{
    if (exchangeLimits.count(symbol) == 0)
        return "";

    return exchangeLimits[symbol].first;
}

std::string NocksSettings::getMaximumForCurrencyAsString(std::string symbol)
{
    if (exchangeLimits.count(symbol) == 0)
        return "";

    return exchangeLimits[symbol].second;
}

void NocksSettings::pollSettings()
{
    QNetworkRequest netRequest;
    netRequest.setUrl(QString::fromStdString("https://www.nocks.com/api/settings"));
    netRequest.setRawHeader("User-Agent", "Gulden-qt");
    netRequest.setRawHeader("Accept", "application/json");

    QSslConfiguration config(QSslConfiguration::defaultConfiguration());
    netRequest.setSslConfiguration(config);

    netManager->get(netRequest);
}

void NocksSettings::netRequestFinished(QNetworkReply* reply)
{
    bool signalUpdates = false;

    if (reply->error() != QNetworkReply::NetworkError::NoError) {

        QTimer::singleShot(30000, this, SLOT(pollSettings()));
    } else {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (statusCode != 200) {

            QTimer::singleShot(30000, this, SLOT(pollSettings()));
        } else {
            QByteArray jsonReply = reply->readAll();
            QString temp = QString::fromStdString(jsonReply.data());
            QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonReply);
            QJsonObject jsonArray = jsonDoc.object().value("pairs").toObject();

            for (const auto& jsonVal : jsonArray) {
                QJsonObject jsonObject = jsonVal.toObject();
                std::string currencyPair = jsonObject.value("pair").toString().toStdString();
                std::string amountMinimum = jsonObject.value("amountMinimum").toString().toStdString();
                std::string amountMaximum = jsonObject.value("amountMaximum").toString().toStdString();

                exchangeLimits[currencyPair] = std::make_pair(amountMinimum, amountMaximum);

                signalUpdates = true;
            }

            QTimer::singleShot(86400000, this, SLOT(pollSettings()));
        }
    }

    if (signalUpdates) {
        Q_EMIT nocksSettingsUpdated();
    }
}

void NocksSettings::reportSslErrors(QNetworkReply* reply, const QList<QSslError>& errorList)
{
}
