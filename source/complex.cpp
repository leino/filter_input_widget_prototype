namespace Complex
{
    union C
    {
        
        struct Components
        {
            float real;
            float imaginary;
        } component;
        
        float components[2];
        
    };

    inline void
    set_polar(float const radius, float const angle, C *const z)
    {
        z->component.real = radius * Numerics::cos(angle);
        z->component.imaginary = radius * Numerics::sin(angle);        
    }

    void
    unit(C *const z)
    {
        z->component.real = 1.0f;
        z->component.imaginary = 0.0f;
    }

    float
    phase(C const*const z)
    {
        return Numerics::arc_tangent(z->component.real, z->component.imaginary);
    }
    
    void
    multiply(C const*const a, C *const z)
    {
        float const original_real = z->component.real;
        z->component.real =
            a->component.real * z->component.real - a->component.imaginary * z->component.imaginary;
        
        z->component.imaginary =
            a->component.real * z->component.imaginary + a->component.imaginary * original_real;
    }

    inline float
    magnitude_squared(C const*const a)
    {
        using namespace Numerics;
        return square(a->component.real) + square(a->component.imaginary);
    }        
    
    void
    quotient(C const*const n, C const*const d, C *const q)
    {
        float const d_sq = magnitude_squared(d);
        q->component.real =
            (n->component.real*d->component.real + n->component.imaginary*d->component.imaginary)/d_sq;
        q->component.imaginary =
            (n->component.imaginary*d->component.real - n->component.real*d->component.imaginary)/d_sq;
    }
    
    void
    unit_circle_point(float const angle, C *const z)
    {
        z->component.real = Numerics::cos(angle);
        z->component.imaginary = Numerics::sin(angle);
    }
    
    inline void
    conjugate(C const*const a, C *const a_conjugate)
    {
        a_conjugate->component.real = a->component.real;
        a_conjugate->component.imaginary = -a->component.imaginary;
    }

    void
    difference(C const*const a, C const*const b, C *const result)
    {
        result->component.real = a->component.real - b->component.real;
        result->component.imaginary = a->component.imaginary - b->component.imaginary;
    }
    
    inline float
    distance_squared(float const real, float const imaginary, C const*const a)
    {
        using namespace Numerics;
        return square(real - a->component.real) + square(imaginary - a->component.imaginary);
    }


    inline float
    magnitude(C const*const a)
    {
        return Numerics::square_root(magnitude_squared(a));
    }    
    
    inline float
    distance_squared(C const*const a, C const*const b)
    {
        using namespace Numerics;
        return
            square(a->component.real - b->component.real) +
            square(a->component.imaginary - b->component.imaginary);
    }    

    inline float
    distance(C const*const a, C const*const b)
    {
        return Numerics::square_root( distance_squared(a, b) );
    }        
    
}
