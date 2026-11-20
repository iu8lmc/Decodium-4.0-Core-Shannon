QVariantMap DecodiumBridge::sendFt2LinkGatewayEmail(quint32 mailboxId,
                                                    const QVariantMap& config,
                                                    const QString& eml)
{
    static quint32 nextRequestId = 1;
    quint32 const requestId = nextRequestId++;

    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("requestId"), requestId);
    result.insert(QStringLiteral("mailboxId"), mailboxId);

    QString const host = ft2LinkSmtpConfigString(config, QStringLiteral("host"));
    int const port = qBound(1, config.value(QStringLiteral("port"), 587).toInt(), 65535);
    QString security = ft2LinkSmtpConfigString(config, QStringLiteral("security"),
                                               QStringLiteral("STARTTLS")).toUpper();
    if (security != QStringLiteral("TLS")
        && security != QStringLiteral("SSL")
        && security != QStringLiteral("STARTTLS")
        && security != QStringLiteral("NONE")) {
        security = QStringLiteral("STARTTLS");
    }
    QString const username = ft2LinkSmtpConfigString(config, QStringLiteral("username"));
    QString const fromEmail = ft2LinkCleanEmailAddress(
        ft2LinkSmtpConfigString(config, QStringLiteral("fromEmail"), username));
    QString const toEmail = ft2LinkCleanEmailAddress(
        ft2LinkSmtpConfigString(config, QStringLiteral("toEmail")));
    bool const auth = config.value(QStringLiteral("auth"), !username.isEmpty()).toBool();
    bool const testOnly = config.value(QStringLiteral("testOnly"), false).toBool();

    if (host.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing SMTP host"));
        return result;
    }
    if (fromEmail.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing SMTP From email"));
        return result;
    }
    if (!testOnly && toEmail.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing SMTP recipient email"));
        return result;
    }
    if (!testOnly && eml.trimmed().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing EML payload"));
        return result;
    }
    if (auth && username.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("SMTP auth requires username"));
        return result;
    }

    QString password;
    if (auth) {
        QString const account = ft2LinkSmtpSecretKey(config);
        auto const& backend = secure_settings::default_backend();
        if (!backend.available()) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Secure password backend unavailable"));
            return result;
        }
        auto const lookup = backend.lookup(secure_settings::service(m_callsign.trimmed().toUpper()),
                                           account);
        if (!lookup.found) {
            result.insert(QStringLiteral("error"),
                          lookup.error.isEmpty()
                              ? QStringLiteral("SMTP password not saved")
                              : lookup.error);
            return result;
        }
        password = lookup.value;
    }

    struct SmtpState
    {
        enum Stage {
            Greeting,
            EhloBeforeTls,
            StartTls,
            TlsHandshake,
            EhloAfterTls,
            AuthLogin,
            AuthUser,
            AuthPassword,
            MailFrom,
            RcptTo,
            DataCommand,
            DataBody,
            Quit
        };

        QPointer<QSslSocket> socket;
        QPointer<QTimer> timer;
        QByteArray buffer;
        Stage stage {Greeting};
        QString host;
        int port {587};
        QString security;
        QString username;
        QString password;
        QString fromEmail;
        QString toEmail;
        QByteArray emlData;
        bool auth {false};
        bool testOnly {false};
        bool failed {false};
        quint32 requestId {0};
        quint32 mailboxId {0};
        QStringList transcript;
        std::function<void(QString const&, QString const&)> status;
        std::function<void(QString const&)> fail;
        std::function<void()> sendEhlo;
        std::function<void()> beginAuthOrMail;
        std::function<void(QByteArray const&)> writeRaw;
        std::function<void(int, QString const&)> handleReply;
    };

    auto state = std::make_shared<SmtpState>();
    state->socket = new QSslSocket(this);
    state->timer = new QTimer(state->socket);
    state->timer->setSingleShot(true);
    state->timer->setInterval(30000);
    state->host = host;
    state->port = port;
    state->security = security;
    state->username = username;
    state->password = password;
    state->fromEmail = fromEmail;
    state->toEmail = toEmail;
    state->auth = auth;
    state->testOnly = testOnly;
    state->requestId = requestId;
    state->mailboxId = mailboxId;
    state->emlData = ft2LinkSmtpData(eml);

    SmtpState* smtp = state.get();
    state->status = [this, smtp](QString const& smtpState, QString const& detail) {
        emit ft2LinkEmailGatewayStatus(smtp->requestId, smtp->mailboxId, smtpState, detail);
    };
    state->fail = [this, smtp](QString const& detail) {
        if (smtp->failed) {
            return;
        }
        smtp->failed = true;
        smtp->status(QStringLiteral("Failed"), detail);
        emit errorMessage(QStringLiteral("FT2-Link SMTP gateway failed: %1").arg(detail));
        if (smtp->timer) {
            smtp->timer->stop();
        }
        if (smtp->socket) {
            smtp->socket->disconnectFromHost();
            smtp->socket->deleteLater();
        }
    };
    state->writeRaw = [smtp](QByteArray const& bytes) {
        if (smtp->socket && !smtp->failed) {
            smtp->socket->write(bytes);
        }
    };
    state->sendEhlo = [smtp]() {
        smtp->status(QStringLiteral("SMTP"),
                      QStringLiteral("EHLO %1").arg(QSysInfo::machineHostName()));
        smtp->writeRaw(ft2LinkSmtpLine(
            QStringLiteral("EHLO %1").arg(QSysInfo::machineHostName().isEmpty()
                                          ? QStringLiteral("decodium")
                                          : QSysInfo::machineHostName())));
    };
    state->beginAuthOrMail = [smtp]() {
        if (smtp->testOnly && !smtp->auth) {
            smtp->stage = SmtpState::Quit;
            smtp->status(QStringLiteral("Ready"),
                         QStringLiteral("SMTP test OK for %1:%2")
                             .arg(smtp->host)
                             .arg(smtp->port));
            smtp->writeRaw(ft2LinkSmtpLine(QStringLiteral("QUIT")));
        } else if (smtp->auth) {
            smtp->stage = SmtpState::AuthLogin;
            smtp->writeRaw(ft2LinkSmtpLine(QStringLiteral("AUTH LOGIN")));
        } else {
            smtp->stage = SmtpState::MailFrom;
            smtp->writeRaw(ft2LinkSmtpLine(
                QStringLiteral("MAIL FROM:<%1>").arg(smtp->fromEmail)));
        }
    };
    state->handleReply = [smtp](int code, QString const& line) {
        if (smtp->failed) {
            return;
        }
        smtp->transcript << line.left(180);
        auto expect = [smtp, code, &line](std::initializer_list<int> codes,
                                           QString const& action) {
            for (int allowed : codes) {
                if (code == allowed) {
                    return true;
                }
            }
            smtp->fail(QStringLiteral("%1 rejected: %2").arg(action, line));
            return false;
        };

        switch (smtp->stage) {
        case SmtpState::Greeting:
            if (!expect({220}, QStringLiteral("Greeting"))) return;
            smtp->stage = SmtpState::EhloBeforeTls;
            smtp->sendEhlo();
            break;
        case SmtpState::EhloBeforeTls:
            if (!expect({250}, QStringLiteral("EHLO"))) return;
            if (smtp->security == QStringLiteral("STARTTLS")) {
                smtp->stage = SmtpState::StartTls;
                smtp->writeRaw(ft2LinkSmtpLine(QStringLiteral("STARTTLS")));
            } else {
                smtp->beginAuthOrMail();
            }
            break;
        case SmtpState::StartTls:
            if (!expect({220}, QStringLiteral("STARTTLS"))) return;
            smtp->stage = SmtpState::TlsHandshake;
            smtp->status(QStringLiteral("TLS"), QStringLiteral("Starting TLS"));
            smtp->socket->startClientEncryption();
            break;
        case SmtpState::EhloAfterTls:
            if (!expect({250}, QStringLiteral("EHLO after TLS"))) return;
            smtp->beginAuthOrMail();
            break;
        case SmtpState::AuthLogin:
            if (!expect({334}, QStringLiteral("AUTH LOGIN"))) return;
            smtp->stage = SmtpState::AuthUser;
            smtp->writeRaw(smtp->username.toUtf8().toBase64() + "\r\n");
            break;
        case SmtpState::AuthUser:
            if (!expect({334}, QStringLiteral("SMTP username"))) return;
            smtp->stage = SmtpState::AuthPassword;
            smtp->writeRaw(smtp->password.toUtf8().toBase64() + "\r\n");
            break;
        case SmtpState::AuthPassword:
            if (!expect({235}, QStringLiteral("SMTP password"))) return;
            if (smtp->testOnly) {
                smtp->stage = SmtpState::Quit;
                smtp->status(QStringLiteral("Ready"),
                             QStringLiteral("SMTP auth test OK for %1:%2")
                                 .arg(smtp->host)
                                 .arg(smtp->port));
                smtp->writeRaw(ft2LinkSmtpLine(QStringLiteral("QUIT")));
            } else {
                smtp->stage = SmtpState::MailFrom;
                smtp->writeRaw(ft2LinkSmtpLine(
                    QStringLiteral("MAIL FROM:<%1>").arg(smtp->fromEmail)));
            }
            break;
        case SmtpState::MailFrom:
            if (!expect({250}, QStringLiteral("MAIL FROM"))) return;
            smtp->stage = SmtpState::RcptTo;
            smtp->writeRaw(ft2LinkSmtpLine(
                QStringLiteral("RCPT TO:<%1>").arg(smtp->toEmail)));
            break;
        case SmtpState::RcptTo:
            if (!expect({250, 251}, QStringLiteral("RCPT TO"))) return;
            smtp->stage = SmtpState::DataCommand;
            smtp->writeRaw(ft2LinkSmtpLine(QStringLiteral("DATA")));
            break;
        case SmtpState::DataCommand:
            if (!expect({354}, QStringLiteral("DATA"))) return;
            smtp->stage = SmtpState::DataBody;
            smtp->status(QStringLiteral("Sending"),
                          QStringLiteral("Sending VMail email to %1").arg(smtp->toEmail));
            smtp->writeRaw(smtp->emlData);
            break;
        case SmtpState::DataBody:
            if (!expect({250}, QStringLiteral("Message body"))) return;
            smtp->stage = SmtpState::Quit;
            smtp->status(QStringLiteral("Sent"),
                          QStringLiteral("SMTP accepted VMail email for %1").arg(smtp->toEmail));
            smtp->writeRaw(ft2LinkSmtpLine(QStringLiteral("QUIT")));
            if (smtp->timer) {
                smtp->timer->stop();
            }
            break;
        case SmtpState::Quit:
            if (code == 221 || code == 250 || code == 0) {
                if (smtp->timer) {
                    smtp->timer->stop();
                }
                if (smtp->socket) {
                    smtp->socket->disconnectFromHost();
                }
            }
            break;
        case SmtpState::TlsHandshake:
            break;
        }
    };

    QObject::connect(state->timer, &QTimer::timeout, this, [state]() {
        state->fail(QStringLiteral("SMTP timeout"));
    });
    QObject::connect(state->socket, &QSslSocket::connected, this, [state]() {
        state->status(QStringLiteral("Connected"),
                      QStringLiteral("Connected to %1:%2").arg(state->host).arg(state->port));
    });
    QObject::connect(state->socket, &QSslSocket::encrypted, this, [state]() {
        if (state->stage == SmtpState::TlsHandshake) {
            state->stage = SmtpState::EhloAfterTls;
            state->sendEhlo();
        } else {
            state->status(QStringLiteral("TLS"), QStringLiteral("TLS established"));
        }
    });
    QObject::connect(state->socket, &QSslSocket::readyRead, this, [state]() {
        state->buffer += state->socket->readAll();
        for (;;) {
            int const lf = state->buffer.indexOf('\n');
            if (lf < 0) {
                break;
            }
            QByteArray lineBytes = state->buffer.left(lf + 1);
            state->buffer.remove(0, lf + 1);
            while (lineBytes.endsWith('\n') || lineBytes.endsWith('\r')) {
                lineBytes.chop(1);
            }
            QString const line = QString::fromUtf8(lineBytes);
            if (line.size() < 3) {
                continue;
            }
            bool ok = false;
            int const code = line.left(3).toInt(&ok);
            if (!ok) {
                continue;
            }
            bool const finalLine = line.size() < 4 || line.at(3) == QLatin1Char(' ');
            if (finalLine) {
                state->handleReply(code, line);
            }
        }
    });
    QObject::connect(state->socket,
                     qOverload<QAbstractSocket::SocketError>(&QSslSocket::errorOccurred),
                     this,
                     [state](QAbstractSocket::SocketError) {
        if (!state->failed && state->socket) {
            state->fail(state->socket->errorString());
        }
    });
    QObject::connect(state->socket,
                     qOverload<const QList<QSslError>&>(&QSslSocket::sslErrors),
                     this,
                     [state](QList<QSslError> const& errors) {
        if (!errors.isEmpty()) {
            state->fail(QStringLiteral("TLS error: %1")
                        .arg(errors.first().errorString()));
        }
    });
    QObject::connect(state->socket, &QSslSocket::disconnected, state->socket,
                     &QObject::deleteLater);

    state->timer->start();
    state->status(QStringLiteral("Queued"),
                  testOnly
                      ? QStringLiteral("SMTP gateway test queued for %1:%2")
                            .arg(host)
                            .arg(port)
                      : QStringLiteral("SMTP gateway queued for %1").arg(toEmail));
    if (security == QStringLiteral("TLS") || security == QStringLiteral("SSL")) {
        state->socket->connectToHostEncrypted(host, port);
    } else {
        state->socket->connectToHost(host, port);
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("state"), QStringLiteral("Queued"));
    result.insert(QStringLiteral("detail"),
                  testOnly
                      ? QStringLiteral("SMTP gateway test queued for %1:%2")
                            .arg(host)
                            .arg(port)
                      : QStringLiteral("SMTP gateway queued for %1").arg(toEmail));
    return result;
}

