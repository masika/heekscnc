
#ifndef COUNTER_BORE_CLASS_DEFINITION
#define COUNTER_BORE_CLASS_DEFINITION

// CounterBore.h
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "DepthOp.h"
#include "HeeksCNCTypes.h"
#include "CuttingTool.h"
#include <list>
#include <vector>

class CCounterBore;

class CCounterBoreParams{
	
public:
	double m_diameter;		// This is the 'Q' word in the G83 cycle.  How deep to peck each time.
	int m_sort_locations;		// '1' = sort location points prior to generating GCode (to optimize paths)

	void set_initial_values( const int cutting_tool_number );
	void write_values_to_config();
	void GetProperties(CCounterBore* parent, std::list<Property *> *list);
	void WriteXMLAttributes(TiXmlNode* pElem);
	void ReadParametersFromXMLElement(TiXmlElement* pElem);
};

/**
	The CCounterBore class stores a list of symbols (by type/id pairs) of elements that represent the starting point
	of the top of a counterbore hole.  The counterbore is a flat bottom hole into which a Socket Head Cap Screw can
	fit.  This code stems from the concept created by John Thornton in the 'Counterbore GCode Generator' python
	script.  This module doesn't seek to repeat John's code.  Rather, it will build on the pocketing features
	already in HeeksCNC to reproduce the results.

	The idea is to nominate existing objects to determine starting points.  The depth and width of the counterbore
	will be constant for all selected object locations.  If any of the selected objects are, themselves,
	NC operations objects (based on COp class) then any selected tool diameters will be used to select
	the most appropriate screw size.  The scenario being that an operator places points around their model, uses
	these points to create Drilling cycles.  These drilling cycles could, in turn, be used as reference
	points to bore recesses that allow for the head of the Socket Head Cap Screws placed into these
	drilled holes.  Indeed, this creates what is essentially a referential model whereby the location of
	the original elements (points, intersection elements etc.) will be used both for the drill cycles
	and the counterbore holes.  If the base objects (points) are moved, the drill holes and counterbore
	holes move along with them.
 */

class CCounterBore: public CDepthOp {
public:
	/**
		There are all types of 3d point classes around but most of them seem to be in the HeeksCAD code
		rather than in cod that's accessible by the plugin.  I suspect I'm missing something on this
		but, just in case I'm not, here is a special one (just for this class)
	 */
	typedef struct Point3d {
		double x;
		double y;
		double z;
		Point3d( double a, double b, double c ) : x(a), y(b), z(c) { }
		Point3d() : x(0), y(0), z(0) { }

		bool operator==( const Point3d & rhs ) const
		{
			if (x != rhs.x) return(false);
			if (y != rhs.y) return(false);
			if (z != rhs.z) return(false);

			return(true);
		} // End equivalence operator

		bool operator!=( const Point3d & rhs ) const
		{
			return(! (*this == rhs));
		} // End not-equal operator

		bool operator<( const Point3d & rhs ) const
		{
			if (x > rhs.x) return(false);
			if (x < rhs.x) return(true);
			if (y > rhs.y) return(false);
			if (y < rhs.y) return(true);
			if (z > rhs.z) return(false);
			if (z < rhs.z) return(true);

			return(false);	// They're equal
		} // End equivalence operator
	} Point3d;

	/**
		The following two methods are just to draw pretty lines on the screen to represent drilling
		cycle activity when the operator selects the CounterBore Cycle operation in the data list.
	 */
	std::list< Point3d > PointsAround( const Point3d & origin, const double radius, const unsigned int numPoints ) const;

public:
	/**
		Define some data structures to hold references to CAD elements.  We store both the type and id because
			a) the ID values are only relevant within the context of a type.
			b) we don't want to limit this class to PointType elements alone.  We use these
			   symbols to identify pairs of intersecting elements and place a drilling cycle
			   at their intersection.
 	 */
	typedef int SymbolType_t;
	typedef unsigned int SymbolId_t;
	typedef std::pair< SymbolType_t, SymbolId_t > Symbol_t;
	typedef std::list< Symbol_t > Symbols_t;

public:
	//	These are references to the CAD elements whose position indicate where the CounterBore Cycle begins.
	Symbols_t m_symbols;
	CCounterBoreParams m_params;


	// depth and diameter (in that order)
	static std::pair< double, double > SelectSizeForHead( const double drill_hole_diameter );

	//	Constructors.
	CCounterBore():CDepthOp(GetTypeString()){}
	CCounterBore(	const Symbols_t &symbols, 
			const int cutting_tool_number )
		: CDepthOp(GetTypeString(), cutting_tool_number), m_symbols(symbols)
	{
		m_params.set_initial_values( cutting_tool_number );

		std::list<int> drillbits;
		std::vector<CCounterBore::Point3d> locations = FindAllLocations( symbols, &drillbits );
		if (drillbits.size() > 0)
		{
			// We found some drilling objects amongst the symbols. Use the diameter of
			// any tools they used to help decide what size cap screw we're trying to cater for.

			for (std::list<int>::const_iterator drillbit = drillbits.begin(); drillbit != drillbits.end(); drillbit++)
			{
				HeeksObj *object = heeksCAD->GetIDObject( CuttingToolType, *drillbit );
				if (object != NULL)
				{
					std::pair< double, double > screw_size = SelectSizeForHead( ((CCuttingTool *) object)->m_params.m_diameter );
					m_depth_op_params.m_final_depth = screw_size.first;
					m_params.m_diameter = screw_size.second;
				} // End if - then
			} // End for
		} // End if - then
	}


	// HeeksObj's virtual functions
	int GetType()const{return CounterBoreType;}
	const wxChar* GetTypeString(void)const{return _T("CounterBore");}
	void glCommands(bool select, bool marked, bool no_color);

	wxString GetIcon(){if(m_active)return theApp.GetResFolder() + _T("/icons/drilling"); else return COp::GetIcon();}
	void GetProperties(std::list<Property *> *list);
	HeeksObj *MakeACopy(void)const;
	void CopyFrom(const HeeksObj* object);
	void WriteXML(TiXmlNode *root);
	bool CanAddTo(HeeksObj* owner);

	// This is the method that gets called when the operator hits the 'Python' button.  It generates a Python
	// program whose job is to generate RS-274 GCode.
	void AppendTextToProgram(const CFixture *pFixture);

	static HeeksObj* ReadFromXMLElement(TiXmlElement* pElem);

	void AddSymbol( const SymbolType_t type, const SymbolId_t id ) { m_symbols.push_back( Symbol_t( type, id ) ); }
	std::vector<Point3d> FindAllLocations( const CCounterBore::Symbols_t & symbols, std::list<int> *pToolNumbersReferenced ) const;

};




#endif // COUNTER_BORE_CLASS_DEFINITION