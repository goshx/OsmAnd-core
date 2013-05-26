#include "RasterizationStyleEvaluator.h"

#include <cassert>

#include "RasterizationStyleContext.h"
#include "RasterizationRule.h"
#include "MapObject.h"
#include "Logging.h"

OsmAnd::RasterizationStyleEvaluator::RasterizationStyleEvaluator( const RasterizationStyleContext& styleContext_, RasterizationStyle::RulesetType ruleset_, const std::shared_ptr<OsmAnd::Model::MapObject>& mapObject_ /*= std::shared_ptr<OsmAnd::Model::MapObject>()*/ )
    : styleContext(styleContext_)
    , mapObject(mapObject_)
    , ruleset(ruleset_)
{

}

OsmAnd::RasterizationStyleEvaluator::~RasterizationStyleEvaluator()
{
}

void OsmAnd::RasterizationStyleEvaluator::setBooleanValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref, const bool& value )
{
    _values[ref.get()].asInt = value ? 1 : 0;
}

void OsmAnd::RasterizationStyleEvaluator::setIntegerValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref, const int& value )
{
    _values[ref.get()].asInt = value;
}

void OsmAnd::RasterizationStyleEvaluator::setFloatValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref, const float& value )
{
    _values[ref.get()].asFloat = value;
}

void OsmAnd::RasterizationStyleEvaluator::setStringValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref, const QString& value )
{
    bool ok = styleContext.style->lookupStringId(value, _values[ref.get()].asUInt);
    if(!ok)
        _values[ref.get()].asUInt = std::numeric_limits<uint32_t>::max();
}

bool OsmAnd::RasterizationStyleEvaluator::getBooleanValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref ) const
{
    return (_values[ref.get()].asInt == 1);
}

int OsmAnd::RasterizationStyleEvaluator::getIntegerValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref ) const
{
    return _values[ref.get()].asInt;
}

float OsmAnd::RasterizationStyleEvaluator::getFloatValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref ) const
{
    return _values[ref.get()].asFloat;
}

QString OsmAnd::RasterizationStyleEvaluator::getStringValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref ) const
{
    return styleContext.style->lookupStringValue(_values[ref.get()].asUInt);
}

void OsmAnd::RasterizationStyleEvaluator::clearValue( const std::shared_ptr<OsmAnd::RasterizationStyle::ValueDefinition>& ref )
{
    _values.remove(ref.get());
}

bool OsmAnd::RasterizationStyleEvaluator::evaluate( bool fillOutput /*= true*/, bool evaluateChildren /*=true*/ )
{
    auto tagKey = _values[RasterizationStyle::builtinValueDefinitions.INPUT_TAG.get()].asUInt;
    auto valueKey = _values[RasterizationStyle::builtinValueDefinitions.INPUT_VALUE.get()].asUInt;

    auto evaluationResult = evaluate(tagKey, valueKey, fillOutput, evaluateChildren);
    if(evaluationResult)
        return true;

    evaluationResult = evaluate(tagKey, 0, fillOutput, evaluateChildren);
    if(evaluationResult)
        return true;

    evaluationResult = evaluate(0, 0, fillOutput, evaluateChildren);
    if(evaluationResult)
        return true;

    return false;
}

bool OsmAnd::RasterizationStyleEvaluator::evaluate( uint32_t tagKey, uint32_t valueKey, bool fillOutput, bool evaluateChildren )
{
    _values[RasterizationStyle::builtinValueDefinitions.INPUT_TAG.get()].asUInt = tagKey;
    _values[RasterizationStyle::builtinValueDefinitions.INPUT_VALUE.get()].asUInt = valueKey;
    
    const auto& rules = static_cast<const RasterizationStyle*>(styleContext.style.get())->obtainRules(ruleset);
    uint64_t ruleId = RasterizationStyle::encodeRuleId(tagKey, valueKey);
    auto itRule = rules.find(ruleId);
    if(itRule == rules.end())
        return false;
    auto evaluationResult = evaluate(*itRule, fillOutput, evaluateChildren);
    return evaluationResult;
}

bool OsmAnd::RasterizationStyleEvaluator::evaluate( const std::shared_ptr<OsmAnd::RasterizationRule>& rule, bool fillOutput, bool evaluateChildren )
{
    auto itValueDef = rule->_valueDefinitionsRefs.begin();
    auto itValueData = rule->_values.begin();
    for(; itValueDef != rule->_valueDefinitionsRefs.end(); ++itValueDef, ++itValueData)
    {
        const auto& valueDef = *itValueDef;
        const auto& valueData = *itValueData;
        const auto& stackValue = _values[valueDef.get()];

        if(valueDef->type != RasterizationStyle::ValueDefinition::Input)
            continue;

        bool evaluationResult = false;
        if(valueDef == RasterizationStyle::builtinValueDefinitions.INPUT_MINZOOM)
        {
            evaluationResult = valueData.asInt <= stackValue.asInt;
        }
        else if(valueDef == RasterizationStyle::builtinValueDefinitions.INPUT_MAXZOOM)
        {
            evaluationResult = valueData.asInt <= stackValue.asInt;
        }
        else if(valueDef == RasterizationStyle::builtinValueDefinitions.INPUT_ADDITIONAL)
        {
            if(!mapObject)
                evaluationResult;
            else
            {
                const auto& strValue = styleContext.style->lookupStringValue(valueData.asUInt);
                auto equalSignIdx = strValue.indexOf('=');
                if(equalSignIdx >= 0)
                {
                    const auto& tag = strValue.mid(0, equalSignIdx);
                    const auto& value = strValue.mid(equalSignIdx + 1);
                    evaluationResult = mapObject->containsType(tag, value, true);
                }
                else
                    evaluationResult = false;
            }
        }
        else if(valueDef->dataType == RasterizationStyle::ValueDefinition::Float)
        {
            evaluationResult = qFuzzyCompare(valueData.asFloat, stackValue.asFloat);
        }
        else
        {
            evaluationResult = valueData.asInt == stackValue.asInt;
        }

        if(!evaluationResult)
            return false;
    }

    if (fillOutput || evaluateChildren)
    {
        auto itValueDef = rule->_valueDefinitionsRefs.begin();
        auto itValueData = rule->_values.begin();
        for(; itValueDef != rule->_valueDefinitionsRefs.end(); ++itValueDef, ++itValueData)
        {
            const auto& valueDef = *itValueDef;
            const auto& valueData = *itValueData;

            if(valueDef->type != RasterizationStyle::ValueDefinition::Output)
                continue;

            _values[valueDef.get()] = valueData;
        }
    }

    if(evaluateChildren)
    {
        for(auto itChild = rule->_ifElseChildren.begin(); itChild != rule->_ifElseChildren.end(); ++itChild)
        {
            auto evaluationResult = evaluate(*itChild, fillOutput, true);
            if(evaluationResult)
                break;
        }

        for(auto itChild = rule->_ifChildren.begin(); itChild != rule->_ifChildren.end(); ++itChild)
        {
            evaluate(*itChild, fillOutput, true);
        }
    }
    
    return true;
}

void OsmAnd::RasterizationStyleEvaluator::dump( bool input /*= true*/, bool output /*= true*/, const QString& prefix /*= QString()*/ ) const
{
    for(auto itValue = _values.begin(); itValue != _values.end(); ++itValue)
    {
        auto pValueDef = itValue.key();
        const auto& value = itValue.value();

        if(pValueDef->type == RasterizationStyle::ValueDefinition::Input && input || pValueDef->type == RasterizationStyle::ValueDefinition::Output && output)
        {
            auto strType = pValueDef->type == RasterizationStyle::ValueDefinition::Input ? ">" : "<";
            OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%s%s%s = ", prefix.toStdString().c_str(), strType, pValueDef->name.toStdString().c_str());

            switch (pValueDef->dataType)
            {
            case RasterizationStyle::ValueDefinition::Boolean:
                OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%s\n", value.asUInt == 1 ? "true" : "false");
                break;
            case RasterizationStyle::ValueDefinition::Integer:
                OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%d\n", value.asInt);
                break;
            case RasterizationStyle::ValueDefinition::Float:
                OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%f\n", value.asFloat);
                break;
            case RasterizationStyle::ValueDefinition::String:
                OsmAnd::LogPrintf(LogSeverityLevel::Debug, "%s\n", styleContext.style->lookupStringValue(value.asUInt));
                break;
            case RasterizationStyle::ValueDefinition::Color:
                OsmAnd::LogPrintf(LogSeverityLevel::Debug, "#%s\n", QString::number(value.asUInt, 16).toStdString().c_str());
                break;
            }
        }
    }
}