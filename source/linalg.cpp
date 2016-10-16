

namespace Vec2
{

    struct Coordinates
    {
        float x;
        float y;
    };
    
    union Vec2
    {
        float v[2];
        Coordinates coordinates;
    };
    
}

namespace Vec3
{

    struct Coordinates
    {
        float x;
        float y;
        float z;
    };
    
    union Vec3
    {
        float v[3];
        Coordinates coordinates;
    };
}


inline
float lerp_float(float const from, float const to, float const t){
    return t*to + (1.0f-t)*from;
}

namespace Vec2 {

    bool are_equal(Vec2 const& a, Vec2 const& b){
        return
            a.v[0] == b.v[0] && 
            a.v[1] == b.v[1]
            ;
    }
    
    void set_zero(Vec2* a){
        a->v[0] = 0.0f; a->v[1] = 0.0f;
    }

    float length_squared(Vec2* a)
    {
        return a->v[0]*a->v[0] + a->v[1]*a->v[1];
    }

    float length(Vec2* a){
        return sqrtf(length_squared(a));
    }

    void set_scaled(float x, Vec2* a, Vec2* r)
    {
        r->v[0] = x*a->v[0];
        r->v[1] = x*a->v[1];
    }

    void set_negated(Vec2* a, Vec2* r)
    {
        r->v[0] = -a->v[0];
        r->v[1] = -a->v[1];
    }

    
    void set_normalized(Vec2* a, Vec2* r){
        set_scaled(1.0f/length(a), a, r);
    }

    void add(Vec2* a, Vec2* r)
    {
        r->v[0] = a->v[0] + r->v[0];
        r->v[1] = a->v[1] + r->v[1];
    }

    void add_coordinates(float x, float y, Vec2* r)
    {
        r->v[0] = x + r->v[0];
        r->v[1] = y + r->v[1];
    }    

    void subtract(Vec2* a, Vec2* r)
    {
        r->v[0] = r->v[0] - a->v[0];
        r->v[1] = r->v[1] - a->v[1];
    }
    
    
    void negate(Vec2* r)
    {
        r->v[0] = -r->v[0];
        r->v[1] = -r->v[1];
    }

    void scale(float x, Vec2* a){
        a->v[0] *= x; a->v[1] *= x;
    }

    void normalize(Vec2* v){
        scale(1.0f/length(v), v);
    }

    void set_sum(Vec2* a, Vec2* b, Vec2* r){
        r->v[0] = a->v[0] + b->v[0];
        r->v[1] = a->v[1] + b->v[1];
    }

    void set_difference(Vec2* a, Vec2* b, Vec2* r){
        r->v[0] = a->v[0] - b->v[0];
        r->v[1] = a->v[1] - b->v[1];
    }

    float inner_product(Vec2* a, Vec2* b){
        return a->v[0]*b->v[0] + a->v[1]*b->v[1];
    }

    float distance_squared(Vec2* a, Vec2* b)
    {
        return powf(a->v[0] - b->v[0], 2.0f) + powf(a->v[1] - b->v[1], 2.0f);
    }

    float distance_squared_components(float x, float y, Vec2* b)
    {
        return powf(x - b->v[0], 2.0f) + powf(y - b->v[1], 2.0f);
    }    
    
    float distance(Vec2* a, Vec2* b)
    {
        float const dist_sq = distance_squared(a, b);
        return sqrtf( dist_sq );
    }

    float distance_components(float x, float y, Vec2* b)
    {
        float const dist_sq = distance_squared_components(x, y, b);
        return sqrtf( dist_sq );
    }    

    void set_equal(Vec2 const*const a, Vec2* r){
        r->v[0] = a->v[0];
        r->v[1] = a->v[1];
    }

    void set(float const x, float const y, Vec2* r)
    {
        r->v[0] = x;
        r->v[1] = y;
    }

    // The transverse (sticking out of the plane) component of the regular 3-dimensional cross product
    float cross_product(Vec2 const& a, Vec2 const& b){
        return a.v[0]*b.v[1] - a.v[1]*b.v[0];
    }

    void set_linear_combination_2(float a, Vec2* x, float b, Vec2* y, Vec2* r){
        r->v[0] = a*x->v[0] + b*y->v[0];
        r->v[1] = a*x->v[1] + b*y->v[1];
    }
    
    void set_lerp(Vec2* from, Vec2* to, float t, Vec2* r){
        r->v[0] = lerp_float(from->v[0], to->v[0], t);
        r->v[1] = lerp_float(from->v[1], to->v[1], t);
    }

    void assert_equal(Vec2* x, Vec2* y, float max_error_sq = 1.0E-6f)
    {
        float error_sq = distance_squared(x, y);
        bool equal = Numerics::absolute_value( error_sq ) < max_error_sq;
        assert(equal);
    }

    void assert_normalized(Vec2* v, float epsilon_squared = 1.0E-6f)
    {
        float error = Numerics::absolute_value( length_squared(v) - 1.0f );
        bool normalized = error < epsilon_squared;
        assert(normalized);
    }
    
}

namespace Vec3
{


    bool are_equal(Vec3 const& a, Vec3 const& b)
    {
        return
            a.v[0] == b.v[0] && 
            a.v[1] == b.v[1] &&
            a.v[2] == b.v[2]
            ;
    }
    
    void set_zero(Vec3* a)
    {
        a->v[0] = 0.0f;
        a->v[1] = 0.0f;
        a->v[2] = 0.0f;
    }

    float length_squared(Vec3* a)
    {
        return a->v[0]*a->v[0] + a->v[1]*a->v[1] + a->v[2]*a->v[2];
    }

    float length(Vec3* a)
    {
        return sqrtf(length_squared(a));
    }

    void set_scaled(float x, Vec3* a, Vec3* r)
    {
        r->v[0] = x*a->v[0];
        r->v[1] = x*a->v[1];
        r->v[2] = x*a->v[2];
    }
    
    void set_normalized(Vec3* a, Vec3* r)
    {
        set_scaled(1.0f/length(a), a, r);
    }

    void add(Vec3* a, Vec3* r)
    {
        r->v[0] = a->v[0] + r->v[0];
        r->v[1] = a->v[1] + r->v[1];
        r->v[2] = a->v[2] + r->v[2];
    }

    void add_coordinates(float x, float y, float z, Vec3* r)
    {
        r->v[0] = x + r->v[0];
        r->v[1] = y + r->v[1];
        r->v[2] = z + r->v[2];
    }    

    void subtract(Vec3* a, Vec3* r)
    {
        r->v[0] = r->v[0] - a->v[0];
        r->v[1] = r->v[1] - a->v[1];
        r->v[2] = r->v[2] - a->v[2];
    }
    
    void scale(float x, Vec3* a)
    {
        a->v[0] *= x;
        a->v[1] *= x;
        a->v[2] *= x;
    }

    void normalize(Vec3* v)
    {
        scale(1.0f/length(v), v);
    }

    void set_sum(Vec3* a, Vec3* b, Vec3* r)
    {
        r->v[0] = a->v[0] + b->v[0];
        r->v[1] = a->v[1] + b->v[1];
        r->v[2] = a->v[2] + b->v[2];
    }

    void set_difference(Vec3* a, Vec3* b, Vec3* r)
    {
        r->v[0] = a->v[0] - b->v[0];
        r->v[1] = a->v[1] - b->v[1];
        r->v[2] = a->v[2] - b->v[2];
    }

    float inner_product(Vec3* a, Vec3* b)
    {
        return
            a->v[0]*b->v[0] +
            a->v[1]*b->v[1] +
            a->v[2]*b->v[2]
            ;
    }

    float distance_squared(Vec3* a, Vec3* b)
    {
        return
            powf(a->v[0] - b->v[0], 2.0f) +
            powf(a->v[1] - b->v[1], 2.0f) +
            powf(a->v[2] - b->v[2], 2.0f)
            ;
    }

    float distance(Vec3* a, Vec3* b)
    {
        float const dist_sq = distance_squared(a, b);
        return sqrtf( dist_sq );
    }

    void set_equal(Vec3* a, Vec3* r)
    {
        r->v[0] = a->v[0];
        r->v[1] = a->v[1];
        r->v[2] = a->v[2];
    }

    void set(float x, float y, float z, Vec3* r)
    {
        r->v[0] = x;
        r->v[1] = y;
        r->v[2] = z;
    }
    
}
