/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef DOWNLOADTHREAD_H
#define DOWNLOADTHREAD_H

#include <QThread>
#include <QFile>
#include <QTemporaryFile>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <curl/curl.h>
#include <iostream>

#include "misc.h"

class downloadThread : public QThread {
  Q_OBJECT

  private:
    QStringList url_list;
    QMutex mutex;
    QWaitCondition condition;

  signals:
    void downloadFinished(QString url, QString file_path, int return_code, QString errorBuffer);

  public:
    downloadThread(QObject* parent) : QThread(parent){}

    void downloadUrl(const QString& url){
      mutex.lock();
      url_list << url;
      mutex.unlock();
      if(!isRunning()){
        start();
      }
    }

    void run(){
      forever{
        mutex.lock();
        if(url_list.size() != 0){
          QString url = url_list.takeFirst();
          mutex.unlock();
          CURL *curl;
          std::string filePath;
          int return_code;
          // XXX: Trick to get a unique filename
          QTemporaryFile *tmpfile = new QTemporaryFile;
          if (tmpfile->open()) {
            filePath = tmpfile->fileName().toStdString();
          }
          delete tmpfile;
          FILE *file = fopen(filePath.c_str(), "w");
          if(!file){
            std::cout << "Error: could not open temporary file...\n";
            return;
          }
          // Initilization required by libcurl
          curl = curl_easy_init();
          if(!curl){
            std::cout << "Error: Failed to init curl...\n";
            fclose(file);
            return;
          }
          // Set url to download
          curl_easy_setopt(curl, CURLOPT_URL, (void*) url.toStdString().c_str());
          // Define our callback to get called when there's data to be written
          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, misc::my_fwrite);
          // Set destination file
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
          // Some SSL mambo jambo
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
          // We want error message:
          char errorBuffer[CURL_ERROR_SIZE];
          return_code = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
          if(return_code){
            std::cout << "Error: failed to set error buffer in curl\n";
            fclose(file);
            QFile::remove(filePath.c_str());
            return;
          }
          // Perform Download
          return_code = curl_easy_perform(curl);
          // Cleanup
          curl_easy_cleanup(curl);
          // Close tmp file
          fclose(file);
          emit downloadFinished(url, QString(filePath.c_str()), return_code, QString(errorBuffer));
        }else{
          mutex.unlock();
          break;
        }
      }
    }
};

#endif
