// TODO: move to own file
namespace Numerics
{

    inline float
    logarithm(float const base, float const x)
    {
        return logf(x)/logf(base);
    }

    inline float
    ceiling(float const x)
    {
        return ceilf(x);
    }

    inline float
    floor(float const x)
    {
        return floorf(x);
    }    

    inline float
    arc_tangent(float const x, float const y)
    {
        return atan2f(y, x);
    }
    
    inline float
    lerp(float const from, float const to, float const t)
    {
        return from*(1.0f - t) + to*t;
    }
    
    inline float
    square(float const a)
    {
        return a*a;
    }

    inline float
    square_root(float const a)
    {
        // TODO: intrinsics?
        return sqrtf(a);
    }    
    
    inline float
    power(float x, float power)
    {
        // TODO: intrinsics!
        return powf(x, power);
    }

    inline int
    power(int x, int power)
    {
        // TODO: intrinsics!
        int p = 1;
        for(int i=0; i<power; i++)
        {
            p *= x;
        }
        return p;
    }    

    inline int64
    power(int64 x, int64 power)
    {
        // TODO: intrinsics!
        int64 p = 1;
        for(int i=0; i<power; i++)
        {
            p *= x;
        }
        return p;
    }
    
    inline uint
    power(uint x, uint power)
    {
        // TODO: intrinsics!
        uint p = 1;
        for(uint i=0; i<power; i++)
        {
            p *= x;
        }
        return p;
    }        
    
    inline float
    sin(float x)
    {
        // TODO: intrinsics!
        return sinf(x);
    }


    inline float
    cos(float x)
    {
        // TODO: intrinsics!
        return cosf(x);
    }
    
    inline float
    minimum(float x, float y)
    {
        // TODO: intrinsics!
        if( x < y )
            return x;
        else
            return y;
    }

    inline int
    minimum(int x, int y)
    {
        // TODO: intrinsics!
        if( x < y )
            return x;
        else
            return y;
    }    
    
    inline float
    maximum(float x, float y)
    {
        // TODO: intrinsics!
        if( x > y )
            return x;
        else
            return y;
    }    

    inline int
    maximum(int x, int y)
    {
        // TODO: intrinsics!
        if( x > y )
            return x;
        else
            return y;
    }    

    inline int
    maximum(int x, int y, int z)
    {
        return maximum(x, maximum(y, z));
    }

    inline int64
    absolute_value(int64 x)
    {
        if(x < 0)
            return -x;
        else
            return +x;
    }

    inline int
    absolute_value(int x)
    {
        if(x < 0)
            return -x;
        else
            return +x;
    }    
    
    inline float
    absolute_value(float x)
    {
        // TODO: intrinsics
        return fabsf(x);
    }

    inline float
    sign(float x)
    {
        // TODO: intrinsics
        if( x > 0.0f )
            return 1.0f;
        else if( x < 0.0f )
            return -1.0f;
        else
            return 0.0f;
    }
    
    
    inline bool
    has_infinite_magnitude_float(float x)
    {
        return x == POSITIVE_INFINITY_FLOAT || x == NEGATIVE_INFINITY_FLOAT;
    }

    // NOTE:
    // Unlike the C/C++ standards '%' operator, we use the convention that 0 <= modulo(x, modulus) < modulus
    // which is very convenient for cyclicly looking up stuff in C/C++ arrays...
    inline int
    remainder(int modulus, int x)
    {
        int r = x % modulus;
        if(r < 0)
        {
            return modulus + r;
        }
        else
        {
            return r;
        }
    }

    // NOTE:
    // Unlike the C/C++ standards '%' operator, we use the convention that 0 <= modulo(x, modulus) < modulus
    // which is very convenient for cyclicly looking up stuff in C/C++ arrays...
    inline int64
    remainder(int64 modulus, int64 x)
    {
        int64 r = x % modulus;
        if(r < 0)
        {
            return modulus + r;
        }
        else
        {
            return r;
        }
    }    
    
    inline uint
    remainder(uint modulus, uint x)
    {
        return x % modulus;
    }    
    
    inline int
    sign(int x)
    {
        if( x > 0 )
        {
            return 1;
        }
        else if( x < 0 )
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }


    inline float
    clamp(float lo, float hi, float x)
    {
        return maximum(lo, minimum(x, hi));
    }

    inline int
    clamp(int lo, int hi, int x)
    {
        return maximum(lo, minimum(x, hi));
    }

    inline float
    smoothstep(float lo, float hi, float x)
    {
        x = clamp( 0.0f, 1.0f, (x-lo)/(hi-lo) );
        float r = (3.0f - 2.0f*x)*x*x;
        return r;
    }

    inline float
    smoothstep_derivative(float lo, float hi, float x)
    {
        x = clamp( 0.0f, 1.0f, (x-lo)/(hi-lo) );
        float r = (6.0f - 6.0f*x)*x;
        return r;
    }    
    
}
