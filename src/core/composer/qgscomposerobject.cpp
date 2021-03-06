/***************************************************************************
                            qgscomposerobject.cpp
                             -------------------
    begin                : July 2014
    copyright            : (C) 2014 by Nyall Dawson,Radim Blazek
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QPainter>

#include "qgscomposition.h"
#include "qgscomposerutils.h"
#include "qgscomposerobject.h"
#include "qgsdatadefined.h"

#define FONT_WORKAROUND_SCALE 10 //scale factor for upscaling fontsize and downscaling painter

#ifndef M_DEG2RAD
#define M_DEG2RAD 0.0174532925
#endif

QgsComposerObject::QgsComposerObject( QgsComposition* composition )
    : QObject( 0 )
    , mComposition( composition )
{

  // data defined strings
  mDataDefinedNames.insert( QgsComposerObject::TestProperty, QString( "dataDefinedTestProperty" ) );

  if ( mComposition )
  {
    //connect to atlas toggling on/off and coverage layer and feature changes
    //to update data defined values
    connect( &mComposition->atlasComposition(), SIGNAL( toggled( bool ) ), this, SLOT( refreshDataDefinedProperty() ) );
    connect( &mComposition->atlasComposition(), SIGNAL( coverageLayerChanged( QgsVectorLayer* ) ), this, SLOT( refreshDataDefinedProperty() ) );
    connect( &mComposition->atlasComposition(), SIGNAL( featureChanged( QgsFeature* ) ), this, SLOT( refreshDataDefinedProperty() ) );
    //also, refreshing composition triggers a recalculation of data defined properties
    connect( mComposition, SIGNAL( refreshItemsTriggered() ), this, SLOT( refreshDataDefinedProperty() ) );

    //toggling atlas or changing coverage layer requires data defined expressions to be reprepared
    connect( &mComposition->atlasComposition(), SIGNAL( toggled( bool ) ), this, SLOT( prepareDataDefinedExpressions() ) );
    connect( &mComposition->atlasComposition(), SIGNAL( coverageLayerChanged( QgsVectorLayer* ) ), this, SLOT( prepareDataDefinedExpressions() ) );
  }

}

QgsComposerObject::~QgsComposerObject()
{
  qDeleteAll( mDataDefinedProperties );
}

bool QgsComposerObject::writeXML( QDomElement &elem, QDomDocument &doc ) const
{
  if ( elem.isNull() )
  {
    return false;
  }

  //data defined properties
  QgsComposerUtils::writeDataDefinedPropertyMap( elem, doc, &mDataDefinedNames, &mDataDefinedProperties );

  //custom properties
  mCustomProperties.writeXml( elem, doc );

  return true;
}

bool QgsComposerObject::readXML( const QDomElement &itemElem, const QDomDocument &doc )
{
  Q_UNUSED( doc );
  if ( itemElem.isNull() )
  {
    return false;
  }

  //data defined properties
  QgsComposerUtils::readDataDefinedPropertyMap( itemElem, &mDataDefinedNames, &mDataDefinedProperties );

  //custom properties
  mCustomProperties.readXml( itemElem );

  return true;
}

QgsDataDefined *QgsComposerObject::dataDefinedProperty( const QgsComposerObject::DataDefinedProperty property ) const
{
  if ( property == QgsComposerObject::AllProperties || property == QgsComposerObject::NoProperty )
  {
    //bad property requested, don't return anything
    return 0;
  }

  //find corresponding QgsDataDefined and return it
  QMap< QgsComposerObject::DataDefinedProperty, QgsDataDefined* >::const_iterator it = mDataDefinedProperties.find( property );
  if ( it != mDataDefinedProperties.constEnd() )
  {
    return it.value();
  }

  //could not find matching QgsDataDefined
  return 0;
}

void QgsComposerObject::setDataDefinedProperty( const QgsComposerObject::DataDefinedProperty property, const bool active, const bool useExpression, const QString &expression, const QString &field )
{
  if ( property == QgsComposerObject::AllProperties || property == QgsComposerObject::NoProperty )
  {
    //bad property requested
    return;
  }

  bool defaultVals = ( !active && !useExpression && expression.isEmpty() && field.isEmpty() );

  if ( mDataDefinedProperties.contains( property ) )
  {
    QMap< QgsComposerObject::DataDefinedProperty, QgsDataDefined* >::const_iterator it = mDataDefinedProperties.find( property );
    if ( it != mDataDefinedProperties.constEnd() )
    {
      QgsDataDefined* dd = it.value();
      dd->setActive( active );
      dd->setExpressionString( expression );
      dd->setField( field );
      dd->setUseExpression( useExpression );
    }
  }
  else if ( !defaultVals )
  {
    QgsDataDefined* dd = new QgsDataDefined( active, useExpression, expression, field );
    mDataDefinedProperties.insert( property, dd );
  }
}

void QgsComposerObject::repaint()
{
  //nothing to do in base class for now
}

void QgsComposerObject::refreshDataDefinedProperty( const DataDefinedProperty property, const QgsExpressionContext *context )
{
  Q_UNUSED( property );
  Q_UNUSED( context );

  //nothing to do in base class for now
}

bool QgsComposerObject::dataDefinedEvaluate( const DataDefinedProperty property, QVariant &expressionValue, const QgsExpressionContext& context ) const
{
  if ( !mComposition )
  {
    return false;
  }
  return mComposition->dataDefinedEvaluate( property, expressionValue, context, &mDataDefinedProperties );
}

void QgsComposerObject::prepareDataDefinedExpressions() const
{
  QScopedPointer< QgsExpressionContext > context( createExpressionContext() );

  //prepare all QgsDataDefineds
  QMap< DataDefinedProperty, QgsDataDefined* >::const_iterator it = mDataDefinedProperties.constBegin();
  if ( it != mDataDefinedProperties.constEnd() )
  {
    it.value()->prepareExpression( *context.data() );
  }
}

void QgsComposerObject::setCustomProperty( const QString& key, const QVariant& value )
{
  mCustomProperties.setValue( key, value );
}

QVariant QgsComposerObject::customProperty( const QString& key, const QVariant& defaultValue ) const
{
  return mCustomProperties.value( key, defaultValue );
}

void QgsComposerObject::removeCustomProperty( const QString& key )
{
  mCustomProperties.remove( key );
}

QStringList QgsComposerObject::customProperties() const
{
  return mCustomProperties.keys();
}

QgsExpressionContext* QgsComposerObject::createExpressionContext() const
{
  QgsExpressionContext* context = 0;
  if ( mComposition )
  {
    context = mComposition->createExpressionContext();
  }
  else
  {
    context = new QgsExpressionContext();
    context->appendScope( QgsExpressionContextUtils::globalScope() );
    context->appendScope( QgsExpressionContextUtils::projectScope() );
  }
  return context;
}
