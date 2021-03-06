#include "PlainQueryFilter.h"

OsmAnd::PlainQueryFilter::PlainQueryFilter( const uint32_t* zoom, const AreaI* area )
    : _isZoomFiltered(zoom != nullptr)
    , _isAreaFiltered(area != nullptr)
{
    if(_isZoomFiltered)
        _zoom = *zoom;
    if(_isAreaFiltered)
        _area = *area;
}

OsmAnd::PlainQueryFilter::~PlainQueryFilter()
{
}

bool OsmAnd::PlainQueryFilter::acceptsZoom( uint32_t zoom )
{
    if(!_isZoomFiltered)
        return true;
    return _zoom == zoom;
}

bool OsmAnd::PlainQueryFilter::acceptsArea( const AreaI& area )
{
    if(!_isAreaFiltered)
        return true;
    return
        _area.intersects(area) ||
        _area.contains(area) ||
        area.contains(_area);
}

bool OsmAnd::PlainQueryFilter::acceptsPoint( const PointI& point )
{
    if(!_isAreaFiltered)
        return true;
    return _area.contains(point);
}
