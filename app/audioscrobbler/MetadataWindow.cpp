/*
   Copyright 2005-2009 Last.fm Ltd. 
      - Primarily authored by Max Howell, Jono Cole and Doug Mansell

   This file is part of the Last.fm Desktop Application Suite.

   lastfm-desktop is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   lastfm-desktop is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with lastfm-desktop.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "MetadataWindow.h"
#include <lastfm/Artist>
#include <lastfm/XmlQuery>
#include <lastfm/ws.h>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTextBrowser>
#include <QNetworkReply>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QVBoxLayout>
#include "Application.h"


MetadataWindow::MetadataWindow()
{
    setCentralWidget(new QWidget);
    QVBoxLayout* v = new QVBoxLayout(centralWidget());

    v->addWidget(ui.now_playing_source = new QLabel("Now Playing: Last.fm Radio"));
    ui.now_playing_source->setObjectName("now_playing");

    {
        QHBoxLayout* h = new QHBoxLayout();
        QVBoxLayout* v2 = new QVBoxLayout();
        h->addWidget(ui.artist_image = new QLabel);
        ui.artist_image->setObjectName("artist_image");
        h->addStretch( 1 );
        v2->addWidget(ui.title = new QLabel);
        v2->addWidget(ui.album = new QLabel);
        v2->addStretch();
        ui.title->setObjectName("title1");
        ui.title->setWordWrap(true);
        ui.album->setObjectName("title2");
        h->addLayout(v2);
        h->addStretch( 1 );
        v->addLayout(h);
    }

    // listeners, scrobbles, tags:
    {
        QLabel* label;
        QGridLayout* grid = new QGridLayout();
        label = new QLabel(tr("Listeners"));
        label->setObjectName("name");
        label->setProperty("alternate", QVariant(true));
        grid->addWidget(label, 0, 0);
        ui.listeners = new QLabel;
        ui.listeners->setObjectName("value");
        ui.listeners->setProperty("alternate", QVariant(true));
        grid->addWidget(ui.listeners, 0, 2);

        label = new QLabel(tr("Scrobbles"));
        label->setObjectName("name");
        grid->addWidget(label, 1, 0);
        ui.scrobbles = new QLabel;
        ui.scrobbles->setObjectName("value");
        grid->addWidget(ui.scrobbles, 1, 2);

        label = new QLabel(tr("Tagged as"));
        label->setObjectName("name");
        label->setProperty("alternate", QVariant(true));
        label->setAlignment( Qt::AlignTop );
        grid->addWidget(label, 2, 0);
        ui.tags = new QLabel;
        ui.tags->setObjectName("value");
        ui.tags->setProperty("alternate", QVariant(true));
        ui.tags->setWordWrap(true);
        grid->addWidget(ui.tags, 2, 2);
        grid->addWidget( new QWidget( this ), 2, 3);
        grid->setColumnStretch( 1, 1 );
        grid->setColumnStretch( 1, 3 );
        v->addLayout(grid);
    }

    // bio:
    v->addWidget(ui.bio = new QTextBrowser);
    v->setStretchFactor(ui.bio, 1);

    // love, tag, share buttons
    {
        QHBoxLayout* h = new QHBoxLayout;
        h->addStretch();
       
        h->addWidget(ui.love = new QPushButton(tr("love")));
        ui.love->setObjectName("love");
        ui.love->addAction( ((audioscrobbler::Application*)qApp)->loveAction() );
        
        h->addWidget(ui.tag = new QPushButton(tr("tag")));
        ui.tag->setObjectName("tag");
        ui.tag->addAction( ((audioscrobbler::Application*)qApp)->tagAction() );
        
        h->addWidget(ui.share = new QPushButton(tr("share")));
        ui.share->setObjectName("share");
        ui.share->addAction( ((audioscrobbler::Application*)qApp)->shareAction() );
       
        h->addStretch();

        v->addLayout(h);
    }

    v->setSpacing(0);
    v->setMargin(0);


    setWindowTitle(tr("Last.fm Audioscrobbler"));
    resize(252, 500);
}

void
MetadataWindow::onTrackStarted(const Track& t, const Track& previous)
{
    const unsigned short em_dash = 0x2014;
    QString title = QString("%1 ") + QChar(em_dash) + " %2";
    ui.title->setText(title.arg(t.artist()).arg(t.title()));
    ui.album->setText("from " + t.album().title());

    if (t.artist() == previous.artist()) return;

    ui.bio->clear();
    ui.artist_image->clear();
    m_currentTrack = t;
    connect(t.artist().getInfo(), SIGNAL(finished()), SLOT(onArtistGotInfo()));
//    connect(t.album().getInfo(), SIGNAL(finished()), SLOT(onAlbumGotInfo()));
}

void
MetadataWindow::onArtistGotInfo()
{
    XmlQuery lfm = static_cast<QNetworkReply*>(sender())->readAll();

    QString scrobbles = lfm["artist"]["stats"]["playcount"].text();
    QString listeners = lfm["artist"]["stats"]["listeners"].text();
    QString tags;
    foreach(const XmlQuery& e, lfm["artist"]["tags"].children("tag")) {
        if (tags.length()) {
            tags += ", ";
        }
        tags += e["name"].text();
    }

    ui.scrobbles->setText(scrobbles);
    ui.listeners->setText(listeners);
    ui.tags->setText(tags);

    //TODO if empty suggest they edit it
    ui.bio->setHtml(lfm["artist"]["bio"]["content"].text());

    QTextFrame* root = ui.bio->document()->rootFrame();
    QTextFrameFormat f = root->frameFormat();
    f.setMargin(12);
    root->setFrameFormat(f);

    QUrl url = lfm["artist"]["image size=medium"].text();
    QNetworkReply* reply = lastfm::nam()->get(QNetworkRequest(url));
    connect(reply, SIGNAL(finished()), SLOT(onArtistImageDownloaded()));
}

void
MetadataWindow::onArtistImageDownloaded()
{
    QPixmap px;
    px.loadFromData(static_cast<QNetworkReply*>(sender())->readAll());

    QLinearGradient g(QPoint(), px.rect().bottomLeft());
    g.setColorAt( 0.0, QColor(0, 0, 0, 0.11*255));
    g.setColorAt( 1.0, QColor(0, 0, 0, 0.88*255));

    QPainter p(&px);
    p.setCompositionMode(QPainter::CompositionMode_Multiply);
    p.fillRect(px.rect(), g);
    p.end();

    ui.artist_image->setPixmap(px);
}

void
MetadataWindow::onStopped()
{
    ui.bio->clear();
    ui.artist_image->clear();
    ui.title->clear();
    m_currentTrack = Track();
}
