/* Copyright (c) 2012, Julian Straub <jstraub@csail.mit.edu>
 * Licensed under the MIT license. See LICENSE.txt or 
 * http://www.opensource.org/licenses/mit-license.php */

#pragma once

#include "baseMeasure.hpp"

#include <stddef.h>
#include <stdint.h>
#include <typeinfo>

#include <armadillo>

using namespace std;
using namespace arma;

template <class U>
class HDP // : public DP<U>
{
  public:
    HDP(const BaseMeasure<U>& base, double alpha, double omega)
      : mH(base), mAlpha(alpha), mOmega(omega)
    {
      //    cout<<"Creating "<<typeid(this).name()<<endl;
    };

    ~HDP()
    {	};

    // interface mainly for python
    uint32_t addDoc(const Mat<U>& x_i)
    {
      mX.push_back(x_i);
      return mX.size();
    };

    // interface mainly for python
    uint32_t addHeldOut(const Mat<U>& x_i)
    {
      //cout<<"added heldout doc with "<<x_i.size()<<" words"<<endl;
      mX_ho.push_back(x_i);
      return mX_ho.size();
    };

    virtual Row<double> P_x(uint32_t d) const=0;

protected:

    const BaseMeasure<U>& mH; // base measure
    double mAlpha; 
    double mOmega;
    vector<Mat<U> > mX; // training data
    vector<Mat<U> > mX_ho; //  held out data

private:

};

