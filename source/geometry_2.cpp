namespace Geometry2
{
    bool
    rectangle_point_intersect(
        float const rectangle_min_x,
        float const rectangle_max_x,
        float const rectangle_min_y,
        float const rectangle_max_y,
        float const point_x,
        float const point_y
        )
    {

        bool const outside =
            point_x < rectangle_min_x || point_x > rectangle_max_x ||
            point_y < rectangle_min_y || point_y > rectangle_max_y
            ;
        
        return !outside;

    }
}
