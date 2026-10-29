/*
  ==============================================================================

    AllColors.h
    Created: 16 Jan 2022 2:29:59pm
    Author:  Joe

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#define USE_LIVE_CONSTANT false

# if USE_LIVE_CONSTANT
# define colorHelper(c) JUCE_LIVE_CONSTANT(c);
# else
# define colorHelper(c) c;
# endif




// [Claude 2026-10-29] The house typeface, one family for the whole window (Joe: "let's do all of these"):
// Segoe UI on Windows, Helvetica Neue on the Mac
inline juce::String houseTypeface()
{
   #if JUCE_WINDOWS
    return "Segoe UI";
   #else
    return "Helvetica Neue";
   #endif
}

namespace FontEditor::CompanyTitle
{
    using namespace juce;
        
    /* Company Title - Justification*/
    inline juce::Justification getJustification()
    {return juce::Justification::centredRight;}

    /* Company Title - Typeface */
    inline juce::String getTypeface()
    {return houseTypeface(); }  // [Claude 2026-10-29] one family everywhere (was "Helvetica")

    /* Company Title - Font Size */
    inline float getFontSize() {
        float fontSize = 13.f;
        return fontSize;
    }

    /* Company Title - Font Style */
    inline juce::Font::FontStyleFlags getFontStyle()
    {return juce::Font::FontStyleFlags::plain; }

    /* Company Title - Font Color */
    inline juce::Colour getFontColor()
    {return juce::Colours::white; }

    /* Company Title - Transparency */
    inline float getFontTransparency()
    {return 0.3f; }
};

namespace FontEditor::ControlLabels
{
    using namespace juce;
        
    /* All Control Labels - Justification */
    inline juce::Justification getJustification()
    {return juce::Justification::centred;}

    /* All Control Labels - Typeface */
    inline juce::String getTypeface()
    {return houseTypeface(); }  // [Claude 2026-10-29] one family everywhere (was "Helvetica")

    /* All Control Labels - Font Size */
    inline float getFontSize() {
        float fontSize = 13.f;
        return fontSize;
    }

    /* All Control Labels - Bold */
    inline juce::Font::FontStyleFlags getFontStyle()
    {
        bool isBold = 0;
        
        if (isBold)
            return juce::Font::bold;
        else return juce::Font::plain;
    }

    /* All Control Labels - Font Color */
    inline juce::Colour getFontColor()
    {return juce::Colours::white; }

    /* All Control Labels - Transparency */
    inline float getFontTransparency()
    {return 0.5f; }
};

namespace FontEditor::ParameterLabels
{
    using namespace juce;
        
    /* All Param Labels - Justification */
    inline juce::Justification getJustification()
    {return juce::Justification::centred;}

    /* All Param Labels - Typeface */
    inline juce::String getTypeface()
    {return houseTypeface(); }  // [Claude 2026-10-29] one family everywhere (was "Helvetica")

    /* All Param Labels - Font Size */
    inline float getFontSize() {
        float fontSize = 12.f;
        return fontSize;
    }

    /* All Param Labels - Bold */
    // [Claude 2026-10-29] Text hierarchy (Joe: "Let's do the text hierarchy"): values plain and clear, labels quieter, band names small and spaced.
    inline juce::Font::FontStyleFlags getFontStyle()
    {
        bool isBold = 0;
        
        if (isBold)
            return juce::Font::bold;
        else return juce::Font::plain;
    }

    /* All Param Labels - Font Color */
    inline juce::Colour getFontColor()
    {return juce::Colour(0xffb4b6bc); }

    /* All Param Labels - Transparency */
    inline float getFontTransparency()
    {return 0.5f; }
};

/* L, M, H next to Control Bars*/
namespace FontEditor::BandLabels
{

};


/* Color Scheme: Window Borders */
namespace ColorScheme::WindowBorders
{
    /* Window Border Color)*/
    inline juce::Colour getWindowBorderColor()
    {return juce::Colour(0xff2d2d2f);}
}

/* Global Color Coordiantion Based on Low, Mid, High Bands */
namespace ColorScheme::BandColors
{
    /* Low Band Color Coordination */
    inline juce::Colour getLowBandBaseColor()
    { return juce::Colour(0xff56bd77); }  // [Claude 2026-10-29] Triad, around the Wonderland green (was 0xff7eff7e)

    /* Mid Band Color Coordination */
    inline juce::Colour getMidBandBaseColor()
    { return juce::Colour(0xff64a6fa); }  // blue (was 0xff7e7efd)

    /* High Band Color Coordination */
    inline juce::Colour getHighBandBaseColor()
    { return juce::Colour(0xffc886dd); }  // purple (was 0xffff7eff)

    /* Bypass Band Color Coordination */
    inline juce::Colour getBypassBaseColor()
    { return juce::Colour(0xff808080); }

    /* [Claude 2026-10-29] The same colour at the perceived brightness of a reference, so the three bands' fills read
       equally bright (green is perceived much brighter than blue at the same HSV value; Joe: "let's do all of these") */
    inline float relativeLuminance(juce::Colour c)
    {
        auto f = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
        return 0.2126f * f(c.getFloatRed()) + 0.7152f * f(c.getFloatGreen()) + 0.0722f * f(c.getFloatBlue());
    }
    inline juce::Colour matchLuminance(juce::Colour c, juce::Colour reference)
    {
        const float target = relativeLuminance(reference);
        float lo = 0.f, hi = 1.f;
        for (int i = 0; i < 18; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            (relativeLuminance(c.withBrightness(mid)) < target ? lo : hi) = mid;
        }
        return c.withBrightness(0.5f * (lo + hi));
    }

    /* Converts Base Color Into Stroked Outline */
    inline juce::Colour makeOutlineColor(juce::Colour baseColor, bool isBypassed=false)
    {   if (!isBypassed)
            return baseColor.withMultipliedBrightness(1.25f);
        else
            return getBypassBaseColor().withMultipliedBrightness(1.25f);
    }

}

namespace ColorScheme::ScrollPadColors
{
    /* Scroll Pad Slider Color */
    inline juce::Colour getScrollPadBaseColor ()
    { return juce::Colour(0xff64a6fa); }  // [Claude 2026-10-29] the mid blue (was 0xff7a97f3)
}

namespace Gradients::FrequencyResponse
{
    inline juce::ColourGradient makeLowRegionGradient(juce::Rectangle<float> bounds, bool isBypassed)
    {
        using namespace juce;
        
        float p1 = 0.25f;
        float p2 = 0.5f;

        float gStep1 = 0.3f;
        float gStep2 = 0.5f;
        float gStep3 = 0.75f;
        
        juce::Colour gradientBaseColor;
        
        if (isBypassed)
            gradientBaseColor = ColorScheme::BandColors::getBypassBaseColor();
        else
            gradientBaseColor = ColorScheme::BandColors::getLowBandBaseColor();
                
        auto gradient = ColourGradient( gradientBaseColor,
                                        bounds.getBottomLeft(),
                                        gradientBaseColor.withMultipliedBrightness(gStep1),
                                        bounds.getTopRight(), true);

        gradient.addColour(p1, gradientBaseColor.withMultipliedBrightness(gStep2));
        gradient.addColour(p2, gradientBaseColor.withMultipliedBrightness(gStep3));

        return gradient;
    }

    inline juce::ColourGradient makeMidRegionGradient(juce::Rectangle<float> bounds, bool isBypassed)
    {
        using namespace juce;
        
        float p1 = 0.33f;
        float p2 = 0.66f;

        float gStep1 = 0.3f;
        float gStep2 = 0.5f;
        float gStep3 = 0.75f;
        
        juce::Colour gradientBaseColor;
        
        if (isBypassed)
            gradientBaseColor = ColorScheme::BandColors::getBypassBaseColor();
        else
            gradientBaseColor = ColorScheme::BandColors::getMidBandBaseColor();
                
        auto gradient = ColourGradient( gradientBaseColor,
                                        bounds.getBottomLeft(),
                                        gradientBaseColor.withMultipliedBrightness(gStep1),
                                        bounds.getTopRight(), true);

        gradient.addColour(p1, gradientBaseColor.withMultipliedBrightness(gStep2));
        gradient.addColour(p2, gradientBaseColor.withMultipliedBrightness(gStep3));

        return gradient;
    }

    inline juce::ColourGradient makeHighRegionGradient(juce::Rectangle<float> bounds, bool isBypassed)
    {
        using namespace juce;
        
        float p1 = 0.25f;
        float p2 = 0.5f;
        
        float gStep1 = 0.3f;
        float gStep2 = 0.5f;
        float gStep3 = 0.75f;
        
        juce::Colour gradientBaseColor;
        
        if (isBypassed)
            gradientBaseColor = ColorScheme::BandColors::getBypassBaseColor();
        else
            gradientBaseColor = ColorScheme::BandColors::getHighBandBaseColor();
        
        auto gradient = ColourGradient( gradientBaseColor,
                                        bounds.getBottomRight(),
                                        gradientBaseColor.withMultipliedBrightness(gStep1),
                                        bounds.getTopLeft(), true);

        gradient.addColour(p1, gradientBaseColor.withMultipliedBrightness(gStep2));
        gradient.addColour(p2, gradientBaseColor.withMultipliedBrightness(gStep3));

        return gradient;
    }
}

namespace Gradients::Oscilloscope
{
    inline juce::ColourGradient makeWaveformGradient(juce::Rectangle<float> bounds, juce::String type, bool isBypassed)
    {
        using namespace juce;
        
        juce::Colour gradientBaseColor;
        
        
        if (!isBypassed)
        {
            if (type == "LOW")
                gradientBaseColor = ColorScheme::BandColors::getLowBandBaseColor();
            
            if (type == "MID")
                gradientBaseColor = ColorScheme::BandColors::getMidBandBaseColor();
            
            if (type == "HIGH")
                gradientBaseColor = ColorScheme::BandColors::getHighBandBaseColor();
        }
        else
            gradientBaseColor = ColorScheme::BandColors::getBypassBaseColor();

        
        float p1 = 0.25f;
        float p2 = 0.5f;

        float gStep1 = 0.3f;
        float gStep2 = 0.5f;
        float gStep3 = 0.75f;
        
        auto gradient = ColourGradient( gradientBaseColor,
                                        bounds.getBottomLeft(),
                                        gradientBaseColor.withMultipliedBrightness(gStep1),
                                        bounds.getTopRight(), true);

        gradient.addColour(p1, gradientBaseColor.withMultipliedBrightness(gStep2));
        gradient.addColour(p2, gradientBaseColor.withMultipliedBrightness(gStep3));

        return gradient;
    }
}

namespace Gradients::Icons
{
    inline juce::ColourGradient makeIconGradient(juce::Rectangle<float> bounds, juce::String type)
    {
        using namespace juce;
        
        juce::Colour gradientBaseColor;
        
        if (type == "LOW")
            gradientBaseColor = ColorScheme::BandColors::getLowBandBaseColor();
        
        if (type == "MID")
            gradientBaseColor = ColorScheme::BandColors::getMidBandBaseColor();
        
        if (type == "HIGH")
            gradientBaseColor = ColorScheme::BandColors::getHighBandBaseColor();
        
        float p1 = 0.25f;
        float p2 = 0.5f;
        float p3 = 0.75f;

        float gStep1 = 1.4f;
        float gStep2 = 0.9f;
        float gStep3 = 1.4f;
        
        auto gradient = ColourGradient( gradientBaseColor,
                                        bounds.getBottomLeft(),
                                        gradientBaseColor,
                                        bounds.getTopRight(), true);

        gradient.addColour(p1, gradientBaseColor.withMultipliedBrightness(gStep1));
        gradient.addColour(p2, gradientBaseColor.withMultipliedBrightness(gStep2));
        gradient.addColour(p3, gradientBaseColor.withMultipliedBrightness(gStep3));

        return gradient;
    }
}

namespace Gradients::BarSliders
{
    inline juce::ColourGradient makeStandardSliderGradient(juce::Rectangle<float> bounds, juce::String type)
    {
        using namespace juce;
        
        juce::Colour gradientBaseColor;
        
        if (type == "LOW")
            gradientBaseColor = ColorScheme::BandColors::getLowBandBaseColor();
        
        if (type == "MID")
            gradientBaseColor = ColorScheme::BandColors::getMidBandBaseColor();
        
        if (type == "HIGH")
            gradientBaseColor = ColorScheme::BandColors::getHighBandBaseColor();
        
        // [Claude 2026-10-29] each stop matched to the high band's brightness at the same stop
        const auto ref = ColorScheme::BandColors::getHighBandBaseColor();
        auto even = [&](juce::Colour c, float k) { return ColorScheme::BandColors::matchLuminance(c, ref.withMultipliedBrightness(k)); };
        auto evenB = [&](juce::Colour c, float v) { return ColorScheme::BandColors::matchLuminance(c, ref.withBrightness(v)); };
        // bookmark
        float p1 = 0.3f;
        float p2 = 0.7f;

        float gStep1 = 0.6f;
        float gStep2 = 0.7f;
        
        auto gradient = ColourGradient( even(gradientBaseColor, 0.5f),
                                        bounds.getBottomLeft(),
                                        even(gradientBaseColor, 0.8f),
                                        bounds.getTopRight(), false);

        gradient.addColour(p1, evenB(gradientBaseColor, gStep1));
        gradient.addColour(p2, evenB(gradientBaseColor, gStep2));

        return gradient;
    }

    inline juce::ColourGradient makeCenterSliderGradient(juce::Rectangle<float> bounds, juce::String type)
    {
        using namespace juce;
        
        juce::Colour gradientBaseColor;
        
        if (type == "LOW")
            gradientBaseColor = ColorScheme::BandColors::getLowBandBaseColor();
        
        if (type == "MID")
            gradientBaseColor = ColorScheme::BandColors::getMidBandBaseColor();
        
        if (type == "HIGH")
            gradientBaseColor = ColorScheme::BandColors::getHighBandBaseColor();
        
        
        // [Claude 2026-10-29] each stop matched to the high band's brightness at the same stop
        const auto ref = ColorScheme::BandColors::getHighBandBaseColor();
        auto even = [&](juce::Colour c, float k) { return ColorScheme::BandColors::matchLuminance(c, ref.withMultipliedBrightness(k)); };
        auto evenB = [&](juce::Colour c, float v) { return ColorScheme::BandColors::matchLuminance(c, ref.withBrightness(v)); };
        float p1 = 0.3f;
        float p2 = 0.5f;
        float p3 = 0.8f;

        float gStep1 = 0.75f;
        float gStep2 = 0.35f;
        float gStep3 = 0.75f;
        
        auto gradient = ColourGradient( even(gradientBaseColor, 0.9f),
                                        bounds.getTopLeft(),
                                        even(gradientBaseColor, 0.9f),
                                        bounds.getBottomRight(), false);

        gradient.addColour(p1, evenB(gradientBaseColor, gStep1));
        gradient.addColour(p2, evenB(gradientBaseColor, gStep2));
        gradient.addColour(p3, evenB(gradientBaseColor, gStep3));

        return gradient;
    }

}

namespace Gradients::ScrollPad
{

    inline juce::ColourGradient setScrollBarGradient(juce::Rectangle<float> bounds)
    {
        using namespace juce;
        
        juce::Colour gradientBaseColor = ColorScheme::ScrollPadColors::getScrollPadBaseColor();
        
        float p1 = 0.25f;
        float p2 = 0.5f;
        float p3 = 0.75f;

        float gStep1 = 0.7f;
        float gStep2 = 0.6f;
        float gStep3 = 0.7f;
        
        auto gradient = ColourGradient( gradientBaseColor.withMultipliedBrightness(0.8f),
                                        bounds.getBottomLeft(),
                                        gradientBaseColor.withMultipliedBrightness(0.8f),
                                        bounds.getTopRight(), false);

        gradient.addColour(p1, gradientBaseColor.withBrightness(gStep1));
        gradient.addColour(p2, gradientBaseColor.withBrightness(gStep2));
        gradient.addColour(p3, gradientBaseColor.withBrightness(gStep3));

        return gradient;
    }

}


namespace AllColors
{
	using namespace juce;

	// Color-Scheme for All Universally Band-Color-Coded Items
	namespace BandColors
	{

		// Brightness-Steps For Gradient
		inline float gStep1 = 0.3f;
		inline float gStep2 = 0.5f;
		inline float gStep3 = 0.75f;
		inline float gStep4 = 0.9f;

		// Gradient Color-Scheme For "Low" Color-Coding
		inline Colour GRADIENT_LOW_1()		{ return	Colour(0xff1fcd1f); }
		inline Colour GRADIENT_LOW_2()		{ return	GRADIENT_LOW_1().withMultipliedBrightness(gStep1);  }
		inline Colour GRADIENT_LOW_3()		{ return	GRADIENT_LOW_1().withMultipliedBrightness(gStep2);  }
		inline Colour GRADIENT_LOW_4()		{ return	GRADIENT_LOW_1().withMultipliedBrightness(gStep3);	}
		inline Colour REGION_BORDER_LOW()	{ return	GRADIENT_LOW_1().withMultipliedBrightness(gStep4);  }

		// Gradient Color-Scheme For "Mid" Color-Coding ========
		inline Colour GRADIENT_MID_1()		{ return	Colour(0xffc420c4); }
		inline Colour GRADIENT_MID_2()		{ return	GRADIENT_MID_1().withMultipliedBrightness(gStep1);  }
		inline Colour GRADIENT_MID_3()		{ return	GRADIENT_MID_1().withMultipliedBrightness(gStep2);  }
		inline Colour GRADIENT_MID_4()		{ return	GRADIENT_MID_1().withMultipliedBrightness(gStep3);	}
		inline Colour REGION_BORDER_MID()	{ return	GRADIENT_MID_1().withMultipliedBrightness(gStep4);  }

		// Gradient Color-Scheme For "High" Color-Coding ========
		inline Colour GRADIENT_HIGH_1()		{ return	Colour(0xff4871f4); }
		inline Colour GRADIENT_HIGH_2()		{ return	GRADIENT_HIGH_1().withMultipliedBrightness(gStep1); }
		inline Colour GRADIENT_HIGH_3()		{ return	GRADIENT_HIGH_1().withMultipliedBrightness(gStep2); }
		inline Colour GRADIENT_HIGH_4()		{ return	GRADIENT_HIGH_1().withMultipliedBrightness(gStep3); }
		inline Colour REGION_BORDER_HIGH()	{ return	GRADIENT_HIGH_1().withMultipliedBrightness(gStep4); }

		// Gradient Color-Scheme For "Bypass" Color-Coding ========
		inline Colour GRADIENT_BYPASS_1()		{ return	Colour(0xff808080); }
		inline Colour GRADIENT_BYPASS_2()		{ return	GRADIENT_BYPASS_1().withMultipliedBrightness(gStep1); }
		inline Colour GRADIENT_BYPASS_3()		{ return	GRADIENT_BYPASS_1().withMultipliedBrightness(gStep2); }
		inline Colour GRADIENT_BYPASS_4()		{ return	GRADIENT_BYPASS_1().withMultipliedBrightness(gStep3); }
		inline Colour REGION_BORDER_BYPASS()	{ return	GRADIENT_BYPASS_1().withMultipliedBrightness(gStep4); }

	}


	namespace SliderColors
	{
//
//
		// BYPASS
		inline ColourGradient BASIC_SLIDER_GRADIENT_BYPASS(Rectangle<float> bounds)
		{
			float p1 = 0.25f;
			float p2 = 0.5f;

			auto gradient = ColourGradient(	BandColors::GRADIENT_BYPASS_3(),
											bounds.getBottomRight(),
											BandColors::GRADIENT_BYPASS_4(),
											bounds.getTopLeft(), false);

			gradient.addColour(p1, BandColors::GRADIENT_BYPASS_1());
			gradient.addColour(p2, BandColors::GRADIENT_BYPASS_2());

			return gradient;
		}

		// BYPASS
		inline ColourGradient OPTION_SLIDER_GRADIENT_BYPASS(Rectangle<float> bounds)
		{
			float p1 = 0.25f;
			float p2 = 0.5f;

			// Not A Typo (Gradient 1 Used Twice)
			auto gradient = ColourGradient(	BandColors::GRADIENT_BYPASS_1().withMultipliedBrightness(1.5f),
											bounds.getBottomRight(),
											BandColors::GRADIENT_BYPASS_1().withMultipliedBrightness(1.5f),
											bounds.getTopLeft(), true);

			gradient.addColour(p1, BandColors::GRADIENT_BYPASS_3().withMultipliedBrightness(1.5f));
			gradient.addColour(p2, BandColors::GRADIENT_BYPASS_4().withMultipliedBrightness(1.5f));

			return gradient;
		}

	}


	namespace PluginEditorColors
	{
		// GRADIENT ITEMS FOR MAIN BACKGROUND ==============================================================
		// [Claude 2026-10-29] The window behind the sections is the header's grey (Joe: the gutters and the separator);
		// was black to a red-brown and a blue
		inline Colour GRADIENT1() { return Colour(0xff1a1b20); }
		inline Colour GRADIENT2() { return Colour(0xff1a1b20); }
		inline Colour GRADIENT3() { return Colour(0xff1a1b20); }
		inline Colour GRADIENT4() { return Colour(0xff1a1b20); }

		// BACKGROUND GRADIENT ==========================================================
		inline ColourGradient BACKGROUND_GRADIENT(juce::Rectangle<float> bounds)
		{
			float p1 = 0.5f;
			float p2 = 0.75f;

			auto gradient = juce::ColourGradient(	GRADIENT1(),
													bounds.getTopLeft(),
													GRADIENT2(),
													bounds.getBottomRight(), true);

			gradient.addColour(p1, GRADIENT3());
			gradient.addColour(p2, GRADIENT4());

			return gradient;
		}
	};

	namespace GlobalControlsColors
	{

		// Brightness-Steps For Gradient
		inline float gStep1 = 0.3f;
		inline float gStep2 = 0.5f;
		inline float gStep3 = 0.75f;
		inline float gStep4 = 0.9f;

		inline Colour BASE_COLOR() { return Colour(0xff03010d); }
		//inline Colour BASE_COLOR() { return colorHelper(Colours::red); }

		// BACKGROUND GRADIENT ==========================================================
		inline ColourGradient BACKGROUND_GRADIENT(Rectangle<float> bounds)
		{
			float p1 = 0.25f;
			float p2 = 0.5f;

			auto gradient = ColourGradient(	BASE_COLOR(),
											bounds.getCentre(),
											BASE_COLOR().withMultipliedAlpha(gStep1),
											bounds.getTopLeft(), true);

			gradient.addColour( p1, BASE_COLOR().withMultipliedAlpha(gStep3));
			gradient.addColour( p2, BASE_COLOR().withMultipliedAlpha(gStep4));

			return gradient;
		}
	};


	namespace WaveControlsColors
	{
		// Brightness-Steps For Gradient
		inline float gStep1 = 0.3f;
		inline float gStep2 = 0.5f;
		inline float gStep3 = 0.75f;
		inline float gStep4 = 0.9f;

		inline Colour BASE_COLOR() { return Colour(0xff20021d); }
		//inline Colour BASE_COLOR() { return colorHelper(Colours::red); }

		// BACKGROUND GRADIENT ==========================================================
		inline ColourGradient BACKGROUND_GRADIENT(Rectangle<float> bounds)
		{
			float p1 = 0.25f;
			float p2 = 0.5f;

			auto gradient = ColourGradient(	BASE_COLOR(),
											bounds.getCentre(),
											BASE_COLOR().withMultipliedAlpha(gStep1),
											bounds.getTopLeft(), true);

			gradient.addColour( p1, BASE_COLOR().withMultipliedAlpha(gStep3));
			gradient.addColour( p2, BASE_COLOR().withMultipliedAlpha(gStep4));

			return gradient;
		}

		// WINDOW BACKGROUND OPACITY ===========================================================================
		inline float BACKGROUND_OPACITY() { return 1.f; }

		// WINDOW BORDER COLOR =======================================================================
		inline Colour BORDER_OUTLINE_COLOR() { return Colour(0x00000000); }		// Border Color

		// WINDOW BORDER THICKNESS ====================================================================
		inline float BORDER_OUTLINE_THICKNESS() { return 3.f; }

		inline Colour TEXT_BACKGROUND_FILL() {return Colour(0xffffffff); }	// Text Background Fill
};


	namespace TimingControlsColors
	{
		// WINDOW BACKGROUND GRADIENT COLORS ==============================================================
			//inline Colour GRADIENT1() { return colorHelper(Colours::red); }
			//inline Colour GRADIENT2() { return colorHelper(Colours::green); }
			//inline Colour GRADIENT3() { return colorHelper(Colours::blue); }
			//inline Colour GRADIENT4() { return colorHelper(Colours::yellow); }

		inline Colour GRADIENT1() { return Colour(0xff4f544f); }
		inline Colour GRADIENT2() { return Colour(0xffd9d6d6); }
		inline Colour GRADIENT3() { return Colour(0xff919990); }
		inline Colour GRADIENT4() { return Colour(0xff95958c); }

		// WINDOW BACKGROUND GRADIENT ==========================================================
		inline ColourGradient BACKGROUND_GRADIENT(Rectangle<float> bounds)
		{
			float p1 = 0.3f;
			float p2 = 0.7f;

			auto gradient = ColourGradient(	GRADIENT1(),
											bounds.getTopLeft(),
											GRADIENT2(),
											bounds.getBottomRight(), false);

			gradient.addColour(p1, GRADIENT3());
			gradient.addColour(p2, GRADIENT4());

			return gradient;
		}

		// WINDOW BACKGROUND OPACITY ===========================================================================
		inline float BACKGROUND_OPACITY() { return 0.75f; }

		// WINDOW BORDER COLOR =======================================================================
		inline Colour BORDER_OUTLINE_COLOR() { return Colour(0x00000000); }		// Border Color

		// WINDOW BORDER THICKNESS ====================================================================
		inline float BORDER_OUTLINE_THICKNESS() { return 3.f; }

		inline Colour TEXT_BACKGROUND_FILL() {return Colour(0xffffffff); }	// Text Background Fill
	};


	namespace GainControlsColors
	{
		inline Colour GRADIENT1() { return Colour(0xff4f544f); }
		inline Colour GRADIENT2() { return Colour(0xffd9d6d6); }
		inline Colour GRADIENT3() { return Colour(0xff919990); }
		inline Colour GRADIENT4() { return Colour(0xff95958c); }

		// WINDOW BACKGROUND GRADIENT ==========================================================
		inline ColourGradient BACKGROUND_GRADIENT(Rectangle<float> bounds)
		{
			float p1 = 0.3f;
			float p2 = 0.7f;

			auto gradient = ColourGradient(	GRADIENT1(),
											bounds.getTopLeft(),
											GRADIENT2(),
											bounds.getBottomRight(), false);

			gradient.addColour(p1, GRADIENT3());
			gradient.addColour(p2, GRADIENT4());

			return gradient;
		}

		// WINDOW BACKGROUND OPACITY ===========================================================================
		inline float BACKGROUND_OPACITY() { return 0.75f; }

		// WINDOW BORDER COLOR =======================================================================
		inline Colour BORDER_OUTLINE_COLOR() { return Colour(0x00000000); }		// Border Color

		// WINDOW BORDER THICKNESS ====================================================================
		inline float BORDER_OUTLINE_THICKNESS() { return 3.f; }

		inline Colour TEXT_BACKGROUND_FILL() { return Colour(0xffffffff); }	// Text Background Fill
	};


	namespace CrossoverControlsColors
	{
		// BACKGROUND GRADIENT COLORS
		inline Colour GRADIENT1() { return Colour(0xff4f544f); }
		inline Colour GRADIENT2() { return Colour(0xffd9d6d6); }
		inline Colour GRADIENT3() { return Colour(0xff919990); }
		inline Colour GRADIENT4() { return Colour(0xff95958c); }

		// WINDOW BACKGROUND GRADIENT ==========================================================
		inline ColourGradient BACKGROUND_GRADIENT(Rectangle<float> bounds)
		{
			float p1 = 0.25f;
			float p2 = 0.5f;

			auto gradient = ColourGradient(	GRADIENT1(),
											bounds.getTopLeft(),
											GRADIENT2(),
											bounds.getBottomRight(), false);

			gradient.addColour(p1, GRADIENT3());
			gradient.addColour(p2, GRADIENT4());

			return gradient;
		}

		// WINDOW BACKGROUND OPACITY ===========================================================================
		inline float BACKGROUND_OPACITY() { return 0.75f; }

		// WINDOW BORDER COLOR =======================================================================
		inline Colour BORDER_OUTLINE_COLOR() { return Colour(0x00000000); }		// Border Color

		// WINDOW BORDER THICKNESS ====================================================================
		inline float BORDER_OUTLINE_THICKNESS() { return 3.f; }

		inline Colour TEXT_BACKGROUND_FILL() { return Colour(0xffffffff); }	// Text Background Fill
	};



	namespace OscilloscopeColors
	{
		// BACKGROUND FILL ==========================================================

		// Brightness-Steps For Gradient
		inline float gStep1 = 0.3f;
		inline float gStep2 = 0.5f;
		inline float gStep3 = 0.75f;
		inline float gStep4 = 0.9f;

		inline Colour BASE_COLOR() { return Colour(0xff03010d); }  // [Claude 2026-10-29] the crossover's near-black, so the two displays match (Joe); was 0xff181212


		inline ColourGradient BACKGROUND_GRADIENT(Rectangle<float> bounds)
		{
			float p1 = 0.25f;
			float p2 = 0.5f;

			auto gradient = ColourGradient(	BASE_COLOR(),
											bounds.getCentre(),
											BASE_COLOR().withMultipliedAlpha(gStep1),
											bounds.getTopLeft(), true);

			gradient.addColour( p1, BASE_COLOR().withMultipliedAlpha(gStep3));
			gradient.addColour( p2, BASE_COLOR().withMultipliedAlpha(gStep4));

			return gradient;
		}

		// CURSOR =====================================================================================
		const Colour CURSOR_LINE (255, 255, 255);	// Cursor



		// BUTTONS ===================================================================================
		const Colour BUTTON_BACKGROUND_FILL (255, 255, 255);
		const Colour BUTTON_TEXT_COLOR(0, 0, 0);
	};


	namespace FrequencyResponseColors
	{

		// BACKGROUND FILL =========================================================

		// Brightness-Steps For Gradient
		inline float gStep1 = 0.3f;
		inline float gStep2 = 0.5f;
		inline float gStep3 = 0.75f;
		inline float gStep4 = 0.9f;

		inline Colour BASE_COLOR() { return Colour(0xff03010d); }

		inline ColourGradient BACKGROUND_GRADIENT(Rectangle<float> bounds)
		{
			float p1 = 0.25f;
			float p2 = 0.5f;

			auto gradient = ColourGradient(	BASE_COLOR(),
											bounds.getCentre(),
											BASE_COLOR().withMultipliedAlpha(gStep1),
											bounds.getTopLeft(), true);

			gradient.addColour( p1, BASE_COLOR().withMultipliedAlpha(gStep3));
			gradient.addColour( p2, BASE_COLOR().withMultipliedAlpha(gStep4));

			return gradient;
		}

	};
}
