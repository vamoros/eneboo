/***************************************************************************
 AQApplication.cpp
 -------------------
 begin                : 29/03/2011
 copyright            : (C) 2003-2011 by InfoSiAL S.L.
 email                : mail@infosial.com
 ***************************************************************************/
/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; version 2 of the License.               *
 ***************************************************************************/
/***************************************************************************
 Este  programa es software libre. Puede redistribuirlo y/o modificarlo
 bajo  los  t�rminos  de  la  Licencia  P�blica General de GNU   en  su
 versi�n 2, publicada  por  la  Free  Software Foundation.
 ***************************************************************************/

#include <qdom.h>

#include <stdio.h>

#include "FLSqlDatabase.h"
#include "FLManager.h"
#include "FLManagerModules.h"
#include "FLDiskCache.h"
#include "FLSettings.h"
#include "FLAccessControlLists.h"

#include "AQApplication.h"
#include "AQApplication_p.h"
#include "AQSObjectFactory.h"
#include "AQSWrapperFactory.h"
#include "AQConfig.h"

FLApplication *aqApp = 0;

AQApplication::AQApplication(int &argc, char **argv)
  : FLApplication(argc, argv)
{
  d = new AQApplicationPrivate;
}

AQApplication::~AQApplication()
{
  delete d;
}

void AQApplication::init(const QString &n, const QString &callFunction,
                         const QString &arguments, bool quitAfterCall,
                         bool noMax)
{
  d->oldApi_ = FLSettings::readBoolEntry("application/oldApi", true);
  if (d->oldApi_ || !n.isEmpty() || (!callFunction.isEmpty() && quitAfterCall)) {
    FLApplication::init(n, callFunction, arguments, quitAfterCall, noMax);
    return;
  }
  printf("AQApplication::init  . . . . n:%s callFn:%s args:%s quit:%d nomax:%d\n",n.ascii(), callFunction.ascii(), arguments.ascii(), quitAfterCall, noMax);
  
  QPixmap::setDefaultOptimization(QPixmap::BestOptim);
  qInitNetworkProtocols();
  FLDiskCache::init(this);
  AQ_DISKCACHE_CLR();

#ifdef QSDEBUGGER
  project_ = new QSProject(this, db()->database());
#else
  project_ = new QSProject(0, db()->database());
#endif

  printf("AQApplication::init - 1 - LOAD\n");
  initializing_ = true;
  AQ_SET_MNGLOADER

  db()->manager()->init();
  mngLoader_->init();
  db()->manager()->loadTables();
  mngLoader_->loadKeyFiles();
  mngLoader_->loadAllIdModules();
  mngLoader_->loadIdAreas();

  printf("AQApplication::init - 2 - ACL\n");
  acl_ = new FLAccessControlLists();
  acl_->init();

  loadScripts();
  mngLoader_->setShaLocalFromGlobal();
  loadTranslations();

  printf("AQApplication::init - 3 - Interpreter\n");
  QSInterpreter *i = project_->interpreter();
  if (i) {
    i->addObjectFactory(flFactory_);
    i->addObjectFactory(new AQSObjectFactory);
    i->addWrapperFactory(new AQSWrapperFactory);
    i->addObjectFactory(new QSInputDialogFactory);
    i->addObjectFactory(new QSUtilFactory);
    i->setErrorMode(QSInterpreter::Notify);
  }

  printf("AQApplication::init - 4 - aqapplication.qs\n");
  d->aqAppScriptObject_ = new QObject(this, "aqAppScript");
  project_->addObject(d->aqAppScriptObject_);
  d->aqAppScript_ = project_->createScript(
                      d->aqAppScriptObject_,
                      mngLoader_->contentCode("aqapplication.qs")
                    );

  printf("AQApplication::init - 5 - Main\n");
  QStringList list(QStringList::split(':', arguments, false));
  QSArgumentList argList(callFunction);
  for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
    argList.append(QSArgument(*it));
  FLApplication::call("aqAppScriptMain", argList, d->aqAppScriptObject_);
  QTimer::singleShot(0, this, SLOT(callInitScript()));
  printf("AQApplication::init - 6 - finish\n");

  AQ_UNSET_MNGLOADER
  initializing_ = false;

  printf("AQApplication::init - 7 - end\n");
  connect(project_, SIGNAL(projectEvaluated()), this, SLOT(callReinitScript()));
}

void AQApplication::reinit()
{
  if (d->oldApi_) {
    FLApplication::reinit();
    return;
  }

  if (initializing_)
    return;
  initializing_ = true;
  writeState();

  db()->managerModules()->finish();
  db()->manager()->finish();

  db()->manager()->init();
  db()->managerModules()->init();
  db()->manager()->cleanupMetaData();

  if (acl_)
    acl_->init();

  loadScripts();
  db()->managerModules()->setShaLocalFromGlobal();

  QTimer::singleShot(0, this, SLOT(callReinitScript()));
  QTimer::singleShot(0, this, SLOT(callScriptEntryFunction()));

  initializing_ = false;
}

void AQApplication::callInitScript()
{
  printf("AQApplication::callInitScript - init\n");

  FLApplication::call("initScript", QSArgumentList(), d->aqAppScriptObject_);
  printf("AQApplication::callInitScript - end\n");
}

void AQApplication::callReinitScript()
{
  FLApplication::call("reinitScript", QSArgumentList(), d->aqAppScriptObject_);
}

void AQApplication::setMainWidget(QWidget *mainWidget)
{
  if (d->oldApi_)
    FLApplication::setMainWidget(mainWidget);
  else
    QApplication::setMainWidget(mainWidget);
}

QSArgument AQApplication::call(const QString &function,
                               const QString &nameObjectContext) const
{
  return FLApplication::call(function, QSArgumentList(), nameObjectContext);
}
