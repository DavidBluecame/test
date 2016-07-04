#ifndef Y_TEXTURE_H
#define Y_TEXTURE_H

#include <yafray_config.h>
#include "surface.h"

__BEGIN_YAFRAY

class YAFRAYCORE_EXPORT texture_t
{
	public :
		/* indicate wether the the texture is discrete (e.g. image map) or continuous */
		virtual bool discrete() const { return false; }
		/* indicate wether the the texture is 3-dimensional. If not, or p.z (and
		   z for discrete textures) are unused on getColor and getFloat calls */
		virtual bool isThreeD() const { return true; }
		virtual bool isNormalmap() const { return false; }
		virtual colorA_t getColor(const point3d_t &p, bool from_postprocessed=false) const = 0;
		virtual colorA_t getColor(int x, int y, int z, bool from_postprocessed=false) const { return colorA_t(0.f); }
		virtual colorA_t getRawColor(const point3d_t &p, bool from_postprocessed=false) const { return getColor(p, from_postprocessed); }
		virtual colorA_t getRawColor(int x, int y, int z, bool from_postprocessed=false) const { return getColor(x, y, z, from_postprocessed); }
		virtual float getFloat(const point3d_t &p) const { return applyAdjustmentsFloat(getRawColor(p).col2bri()); }
		virtual float getFloat(int x, int y, int z) const { return applyAdjustmentsFloat(getRawColor(x, y, z).col2bri()); }
		/* gives the number of values in each dimension for discrete textures */
		virtual void resolution(int &x, int &y, int &z) const { x=0, y=0, z=0; }
		virtual void getInterpolationStep(float &step) const { step = 0.f; };
		virtual void postProcessedCreate() { };
		virtual void postProcessedBlur(float blur_factor) { };
		void setAdjustments(float intensity, float contrast, float saturation, bool clamp, float factor_red, float factor_green, float factor_blue);
		colorA_t applyAdjustmentsColor(const colorA_t & texCol) const;
		float applyAdjustmentsFloat(float texFloat) const;	
		virtual ~texture_t() {}
	
	protected:
		float adj_intensity = 1.f;
		float adj_contrast = 1.f;
		float adj_saturation = 1.f;
		bool adj_clamp = false;
		float adj_mult_factor_red = 1.f;
		float adj_mult_factor_green = 1.f;
		float adj_mult_factor_blue = 1.f;
		bool adjustments_set = false;
};

inline void angmap(const point3d_t &p, float &u, float &v)
{
	float r = p.x*p.x + p.z*p.z;
	u = v = 0.f;
	if (r > 0.f)
	{
		float phiRatio = M_1_PI * fAcos(p.y);//[0,1] range
		r = phiRatio / fSqrt(r);
		u = p.x * r;// costheta * r * phiRatio
		v = p.z * r;// sintheta * r * phiRatio
	}
}

// slightly modified Blender's own function,
// works better than previous function which needed extra tweaks
inline void tubemap(const point3d_t &p, float &u, float &v)
{
	u = 0;
	v = 1 - (p.z + 1)*0.5;
	float d = p.x*p.x + p.y*p.y;
	if (d>0) {
		d = 1/fSqrt(d);
		u = 0.5*(1 - (atan2(p.x*d, p.y*d) *M_1_PI));
	}
}

// maps a direction to a 2d 0..1 interval
inline void spheremap(const point3d_t &p, float &u, float &v)
{
	float sqrtRPhi = p.x*p.x + p.y*p.y;
	float sqrtRTheta = sqrtRPhi + p.z*p.z;
	float phiRatio;

	u = 0.f;
	v = 0.f;

	if(sqrtRPhi > 0.f)
	{
		if(p.y < 0.f) phiRatio = (M_2PI - fAcos(p.x / fSqrt(sqrtRPhi))) * M_1_2PI;
		else		  phiRatio = fAcos(p.x / fSqrt(sqrtRPhi)) * M_1_2PI;
		u = 1.f - phiRatio;
	}

	v = 1.f - (fAcos(p.z / fSqrt(sqrtRTheta)) * M_1_PI);
}

// maps u,v coords in the 0..1 interval to a direction
inline void invSpheremap(float u, float v, vector3d_t &p)
{
	float theta = v * M_PI;
	float phi = -(u * M_2PI);
	float costheta = fCos(theta), sintheta = fSin(theta);
	float cosphi = fCos(phi), sinphi = fSin(phi);
	p.x = sintheta * cosphi;
	p.y = sintheta * sinphi;
	p.z = -costheta;
}

inline void texture_t::setAdjustments(float intensity, float contrast, float saturation, bool clamp, float factor_red, float factor_green, float factor_blue)
{
	adj_intensity = intensity;  adj_contrast = contrast;  adj_saturation = saturation;  adj_clamp = clamp;
	adj_mult_factor_red = factor_red;  adj_mult_factor_green = factor_green;  adj_mult_factor_blue = factor_blue;
	
	std::stringstream adjustments_stream;

	if(intensity != 1.f)
	{
		adjustments_stream << " intensity=" << intensity; 
		adjustments_set = true;
	}
	if(contrast != 1.f)
	{
		adjustments_stream << " contrast=" << contrast; 
		adjustments_set = true;
	}
	if(saturation != 1.f)
	{
		adjustments_stream << " saturation=" << saturation; 
		adjustments_set = true;
	}
	if(factor_red != 1.f)
	{
		adjustments_stream << " factor_red=" << factor_red; 
		adjustments_set = true;
	}
	if(factor_green != 1.f)
	{
		adjustments_stream << " factor_green=" << factor_green; 
		adjustments_set = true;
	}
	if(factor_blue != 1.f)
	{
		adjustments_stream << " factor_blue=" << factor_blue; 
		adjustments_set = true;
	}
	if(clamp)
	{
		adjustments_stream << " clamping=true"; 
		adjustments_set = true;
	}

	if(adjustments_set)
	{
		Y_VERBOSE << "Texture: modified texture adjustment values:" << adjustments_stream.str() << yendl;
	}
}

inline colorA_t texture_t::applyAdjustmentsColor(const colorA_t & texCol) const
{
	if(!adjustments_set) return texCol;
	
	colorA_t ret = texCol;

	if(adj_intensity != 1.f || adj_contrast != 1.f)
	{
		ret.R = (texCol.R-0.5f) * adj_contrast + adj_intensity-0.5f;
		ret.G = (texCol.G-0.5f) * adj_contrast + adj_intensity-0.5f;
		ret.B = (texCol.B-0.5f) * adj_contrast + adj_intensity-0.5f;
	}

	if(adj_mult_factor_red != 1.f) ret.R *= adj_mult_factor_red;
	if(adj_mult_factor_green != 1.f) ret.G *= adj_mult_factor_green;
	if(adj_mult_factor_blue != 1.f) ret.B *= adj_mult_factor_blue;

	if(adj_clamp) ret.clampRGB01();
	
	if(adj_saturation != 1.f)
	{
		float h = 0.f, s = 0.f, v = 0.f;
		ret.rgb_to_hsv(h, s, v);
		s *= adj_saturation;
		ret.hsv_to_rgb(h, s, v);
		if(adj_clamp) ret.clampRGB01();
	}

	return ret;
}

inline float texture_t::applyAdjustmentsFloat(float texFloat) const
{
	if(adj_intensity == 1.f && adj_contrast == 1.f) return texFloat;
	
	float ret = texFloat;

	if(adj_intensity != 1.f || adj_contrast != 1.f)
	{
		ret = (texFloat-0.5f) * adj_contrast + adj_intensity-0.5f;
	}

	if(adj_clamp)
	{
		if(ret < 0.f) ret = 0.f;
		else if(ret > 1.f) ret = 1.f;
	}
	
	return ret;
}


__END_YAFRAY

#endif // Y_TEXTURE_H
