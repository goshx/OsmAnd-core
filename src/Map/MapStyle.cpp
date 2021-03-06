#include "MapStyle.h"
#include "MapStyle_P.h"

#include <QFileInfo>

#include "MapStyleRule.h"
#include "Logging.h"

const OsmAnd::MapStyleBuiltinValueDefinitions OsmAnd::MapStyle::builtinValueDefinitions;

OsmAnd::MapStyle::MapStyle( MapStyles* styles_, const QString& resourcePath_, const bool& isEmbedded_ )
    : _d(new MapStyle_P(this))
    , styles(styles_)
    , resourcePath(resourcePath_)
    , isEmbedded(isEmbedded_)
    , title(_d->_title)
    , name(_d->_name)
    , parentName(_d->_parentName)
{
    _d->_name = QFileInfo(resourcePath).fileName().replace(".render.xml", "");
}

OsmAnd::MapStyle::~MapStyle()
{
}

bool OsmAnd::MapStyle::isStandalone() const
{
    return parentName.isEmpty();
}

bool OsmAnd::MapStyle::areDependenciesResolved() const
{
    if(isStandalone())
        return true;

    return _d->_parent && _d->_parent->areDependenciesResolved();
}

bool OsmAnd::MapStyle::resolveValueDefinition( const QString& name, std::shared_ptr<const MapStyleValueDefinition>& outDefinition ) const
{
    auto itValueDefinition = _d->_valuesDefinitions.find(name);
    if(itValueDefinition != _d->_valuesDefinitions.end())
    {
        outDefinition = *itValueDefinition;
        return true;
    }

    if(!_d->_parent)
        return false;

    return _d->_parent->resolveValueDefinition(name, outDefinition);
}

bool OsmAnd::MapStyle::resolveAttribute( const QString& name, std::shared_ptr<const MapStyleRule>& outAttribute ) const
{
    auto itAttribute = _d->_attributes.find(name);
    if(itAttribute != _d->_attributes.end())
    {
        outAttribute = *itAttribute;
        return true;
    }

    return false;
}

void OsmAnd::MapStyle::dump( const QString& prefix /*= QString()*/ ) const
{
    OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%sPoint rules:", prefix.toStdString().c_str());
    dump(MapStyleRulesetType::Point, prefix);

    OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%sLine rules:", prefix.toStdString().c_str());
    dump(MapStyleRulesetType::Line, prefix);

    OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%sPolygon rules:", prefix.toStdString().c_str());
    dump(MapStyleRulesetType::Polygon, prefix);

    OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%sText rules:", prefix.toStdString().c_str());
    dump(MapStyleRulesetType::Text, prefix);

    OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%sOrder rules:", prefix.toStdString().c_str());
    dump(MapStyleRulesetType::Order, prefix);
}

void OsmAnd::MapStyle::dump( MapStyleRulesetType type, const QString& prefix /*= QString()*/ ) const
{
    const auto& rules = _d->obtainRules(type);
    for(auto itRuleEntry = rules.begin(); itRuleEntry != rules.end(); ++itRuleEntry)
    {
        auto tag = _d->getTagString(itRuleEntry.key());
        auto value = _d->getValueString(itRuleEntry.key());
        auto rule = itRuleEntry.value();

        OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%sRule [%s (%d):%s (%d)]",
            qPrintable(prefix),
            qPrintable(tag),
            _d->getTagStringId(itRuleEntry.key()),
            qPrintable(value),
            _d->getValueStringId(itRuleEntry.key()));
        rule->dump(prefix);
    }
}
