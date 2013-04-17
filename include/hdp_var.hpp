/* Copyright (c) 2012, Julian Straub <jstraub@csail.mit.edu>
 * Licensed under the MIT license. See LICENSE.txt or 
 * http://www.opensource.org/licenses/mit-license.php */

#pragma once

#include "hdp.hpp"

#include "random.hpp"
#include "baseMeasure.hpp"
//#include "dp.hpp"
#include "probabilityHelpers.hpp"

#include <stddef.h>
#include <stdint.h>
#include <typeinfo>

#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/digamma.hpp>
#include <boost/math/special_functions/beta.hpp>
#include <armadillo>

using namespace std;
using namespace arma;

// this one assumes that the number of words per document is smaller than the dictionary size
class HDP_var: public HDP<uint32_t>
{
  public:

    HDP_var(const BaseMeasure<uint32_t>& base, double alpha, double omega)
      : HDP<uint32_t>(base, alpha, omega), mT(0), mK(0), mNw(0)
    {};

    ~HDP_var()
    {};

    double digamma(double x)
    {
      //http://en.wikipedia.org/wiki/Digamma_function#Computation_and_approximation
      if(x<1e-50){
        //cerr<<"\tdigamma param x near zero: "<<x<<" cutting of"<<endl;
        x=1e-50;
      }
      //double x_sq = x*x;
      //return log(x)-1.0/(2.0*x)-1.0/(12.0*x_sq)+1.0/(12*x_sq*x_sq)-1.0/(252.0*x_sq*x_sq*x_sq);
      return boost::math::digamma(x);
    }

    double ElogBeta(const Mat<double>& lambda, uint32_t k, uint32_t w_dn)
    {
      //if(lambda[k](w_dn)<1e-6){
      //  cout<<"\tlambda[k]("<<w_dn<<") near zero: "<<lambda[k](w_dn)<<endl;
      //}
      return digamma(lambda(k,w_dn)) - digamma(sum(lambda.row(k)));
    }

    double ElogSigma(const Mat<double>& a, uint32_t k)
    {
      double e=digamma(a(k,0)) - digamma(a(k,0) + a(k,1));
      for (uint32_t l=0; l<k; ++l)
        e+=digamma(a(k,1)) - digamma(a(k,0) + a(k,1));
      return e; 
    }


    //bool normalizeLogDistribution(Row<double>& r)
    bool normalizeLogDistribution(arma::subview_row<double> r)
    {
      //r.row(i)=exp(r.row(i));
      //cout<<" r="<<r<<endl;
      double minR = as_scalar(min(r));
      //cout<<" minR="<<minR<<endl;
      if(minR > -100.0) {
        //cout<<" logDenom="<<sum(exp(r),1)<<endl;
        double denom = as_scalar(sum(exp(r),1));
        //cout<<" logDenom="<<denom<<endl;
        r -= log(denom); // avoid division by 0
        //cout<<" r - logDenom="<<r<<endl;
        r = exp(r);
        r /= sum(r);
        //cout<<" exp(r - logDenom)="<<r<<endl;
        return true;
      }else{ // cannot compute this -> set the smallest r to 1.0 and the rest to 0
        double maxR = as_scalar(max(r));
        //cout<<"maxR="<<maxR<<" <-" <<arma::max(r) <<endl;
        uint32_t kMax=as_scalar(find(r==maxR,1));
        //cout<<"maxR="<<maxR<<" kMax="<<kMax<<" <-" <<arma::max(r) <<endl;
        r.zeros();
        r(kMax)=1.0;
        //cout<<" r ="<<r<<endl;
        return false;
      }
    }

    //  double ElogSigma(const vector<double>& a, const vector<double>& b, uint32_t k)
    //  {
    //    double e=digamma(a[k]) - digamma(a[k] + b[k]);
    //    for (uint32_t l=0; l<k; ++l)
    //      e+=digamma(b[k]) - digamma(a[k] + b[k]);
    //    return e; 
    //  };
    //
    void initZeta(Mat<double>& zeta, const Mat<double>& lambda, const Mat<uint32_t>& x_d)
    {
      uint32_t N = x_d.n_rows;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;
      //cerr<<"\tinit zeta"<<endl;
      for (uint32_t i=0; i<T; ++i) {
        for (uint32_t k=0; k<K; ++k) {
          zeta(i,k)=0.0;
          for (uint32_t n=0; n<N; ++n) {
            //if(i==0 && k==0) cout<<zeta(i,k)<<" -> ";
            zeta(i,k) += ElogBeta(lambda, k, x_d(n));
          }
        }
        normalizeLogDistribution(zeta.row(i));
        //        if(normalizeLogDistribution(zeta.row(i)))
        //        {
        //          cerr<<"zeta normally computed"<<endl;
        //        }else{
        //          cerr<<"zeta thresholded"<<endl;
        //        }

        //cout<<" normalized="<<zeta(0,0)<<endl;
      }
      //cerr<<"zeta>"<<endl<<zeta<<"<zeta"<<endl;
      //cerr<<"normalization check:"<<endl<<sum(zeta,1).t()<<endl; // sum over rows
    };

    void initPhi(Mat<double>& phi, const Mat<double>& zeta, const Mat<double>& lambda, const Mat<uint32_t>& x_d)
    {
      uint32_t N = x_d.n_rows;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;
      //cout<<"\tinit phi"<<endl;
      for (uint32_t n=0; n<N; ++n){
        for (uint32_t i=0; i<T; ++i) {
          phi(n,i)=0.0;
          for (uint32_t k=0; k<K; ++k) {
            phi(n,i)+=zeta(i,k)* ElogBeta(lambda, k, x_d(n));
          }
        }
        normalizeLogDistribution(phi.row(n));
        //        if(normalizeLogDistribution(phi.row(n)))
        //        {
        //          cerr<<"phi normally computed"<<endl;
        //        }else{
        //          cerr<<"phi thresholded"<<endl;
        //        }
        //
        //        phi.row(n)=exp(phi.row(n));
        //        double denom = sum(phi.row(n));
        //        if(denom > EPS)
        //        {
        //          phi.row(n)/=denom; // avoid division by 0
        //        }else{
        //          cout<<"Phi Init: denominator too small -> no division!
        //        }
      }
      //cerr<<"phi>"<<endl<<phi<<"<phi"<<endl;
    };

    void updateGamma(Mat<double>& gamma, const Mat<double>& phi)
    {
      uint32_t N = phi.n_rows;
      uint32_t T = phi.n_cols;

      gamma.ones();
      gamma.col(1) *= mAlpha;
      for (uint32_t i=0; i<T; ++i) 
      {
        for (uint32_t n=0; n<N; ++n){
          gamma(i,0) += phi(n,i);
          for (uint32_t j=i+1; j<T; ++j) {
            gamma(i,1) += phi(n,j);
          }
        }
      }
      //cout<<gamma.t()<<endl;
    };

    void updateZeta(Mat<double>& zeta, const Mat<double>& phi, const Mat<double>& a, const Mat<double>& lambda, const Mat<uint32_t>& x_d)
    {
      uint32_t N = x_d.n_rows;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

      for (uint32_t i=0; i<T; ++i){
        //zeta(i,k)=0.0;
        for (uint32_t k=0; k<K; ++k) {
          zeta(i,k) = ElogSigma(a,k);
          //cout<<zeta(i,k)<<endl;
          for (uint32_t n=0; n<N; ++n){
            zeta(i,k) += phi(n,i)*ElogBeta(lambda,k,x_d(n));
          }
        }
        normalizeLogDistribution(zeta.row(i));
        //          if(normalizeLogDistribution(zeta.row(i)))
        //          {
        //            cerr<<"zeta normally computed"<<endl;
        //          }else{
        //            cerr<<"zeta thresholded"<<endl;
        //          }

        //          zeta.row(i)=exp(zeta.row(i));
        //          double denom = sum(zeta.row(i));
        //          if(denom > EPS) zeta.row(i)/=denom; // avoid division by 0
      }
    }


    void updatePhi(Mat<double>& phi, const Mat<double>& zeta, const Mat<double>& gamma, const Mat<double>& lambda, const Mat<uint32_t>& x_d)
    {
      uint32_t N = x_d.n_rows;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

      for (uint32_t n=0; n<N; ++n){
        //phi(n,i)=0.0;
        for (uint32_t i=0; i<T; ++i) {
          phi(n,i) = ElogSigma(gamma,i);
          for (uint32_t k=0; k<K; ++k) {
            phi(n,i) += zeta(i,k)*ElogBeta(lambda,k,x_d(n)) ;
          }
        }
        normalizeLogDistribution(phi.row(n));
        //          if(normalizeLogDistribution(phi.row(n)))
        //          {
        //            cerr<<"phi normally computed"<<endl;
        //          }else{
        //            cerr<<"phi thresholded"<<endl;
        //          }
        //          phi.row(n)=exp(phi.row(n));
        //          double denom = sum(phi.row(n));
        //          if(denom > EPS) phi.row(n)/=denom; // avoid division by 0
      }
    }

    void computeNaturalGradients(Mat<double>& d_lambda, Mat<double>& d_a, const Mat<double>& zeta, const Mat<double>&  phi, double omega, uint32_t D, const Mat<uint32_t>& x_d)
    {
      uint32_t N = x_d.n_rows;
      uint32_t Nw = d_lambda.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

      d_lambda.zeros();
      d_a.zeros();
      for (uint32_t k=0; k<K; ++k) { // for all K corpus level topics
        for (uint32_t i=0; i<T; ++i) {
          Row<double> _lambda(Nw); _lambda.zeros();
          for (uint32_t n=0; n<N; ++n){
            _lambda(x_d(n)) += phi(n,i);
          }
          d_lambda.row(k) += zeta(i,k) * _lambda;
          d_a(k,0) += zeta(i,k);
          for (uint32_t l=k+1; l<K; ++l) {
            d_a(k,1) += zeta(i,l);
          }
        }
        d_lambda.row(k) = D*d_lambda.row(k);
        //cout<<"lambda-nu="<<d_lambda[k].t()<<endl;
        d_lambda.row(k) += ((Dir*)(&mH))->mAlphas;
        //cout<<"lambda="<<d_lambda[k].t()<<endl;
        d_a(k,0) = D*d_a(k,0)+1.0;
        d_a(k,1) = D*d_a(k,1)+omega;
      }
      //cout<<"da="<<d_a<<endl;
    }


    // method for "one shot" computation without storing data in this class
    //  Nw: number of different words
    //  kappa=0.9: forgetting rate
    //  uint32_t T=10; // truncation on document level
    //  uint32_t K=100; // truncation on corpus level
    // S = batch size
    vector<Col<uint32_t> > densityEst(const vector<Mat<uint32_t> >& x, uint32_t Nw, 
        double kappa, uint32_t K, uint32_t T, uint32_t S)
    {

      // From: Online Variational Inference for the HDP
      // TODO: think whether it makes sense to use columns and Mat here
      //double mAlpha = 1;
      //double mGamma = 1;
      //double nu = ((Dir*)(&mH))->mAlpha0; // get nu from vbase measure (Dir)
      //double kappa = 0.75;

      // T-1 gamma draws determine a T dim multinomial
      // T = T-1;

      mT = T;
      mK = K;
      mNw = Nw;

      Mat<double> a(K,2);
      a.ones();
      a.col(1) *= mOmega; 
      uint32_t D=x.size();

      // initialize lambda
      GammaRnd gammaRnd(1.0,1.0);
      Mat<double> lambda(K,Nw);
      for (uint32_t k=0; k<K; ++k){
        for (uint32_t w=0; w<Nw; ++w) lambda(k,w) = gammaRnd.draw();
        lambda.row(k) *= double(D)*100.0/double(K*Nw);
        lambda.row(k) += ((Dir*)(&mH))->mAlphas;
      }

      mZeta.resize(D,Mat<double>());
      mPhi.resize(D,Mat<double>());
      mGamma.resize(D,Mat<double>());
      mPerp.zeros(D);

      vector<Col<uint32_t> > z_dn(D);
      Col<uint32_t> ind = shuffle(linspace<Col<uint32_t> >(0,D-1,D),0);
//#pragma omp parallel private(dd,db)
//#pragma omp parallel private(d,dd,N,zeta,phi,converged,gamma,gamma_prev,o,d_lambda,d_a,ro,i,perp_i)
      //shared(x,mZeta,mPhi,mGamma,mOmega,D,T,K,Nw,mA,mLambda,mPerp,mX_ho)
      {
//#pragma omp for schedule(dynamic)
//#pragma omp for schedule(dynamic) ordered
        for (uint32_t dd=0; dd<D; dd += S)
        {
          Mat<double> db_lambda(K,Nw); // batch updates
          Mat<double> db_a(K,2); 
#pragma omp parallel for schedule(dynamic) 
          for (uint32_t db=dd; db<min(dd+S,D); db++)
          {
            uint32_t d=ind[db];  
            uint32_t N=x[d].n_rows;
            //      cout<<"---------------- Document "<<d<<" N="<<N<<" -------------------"<<endl;

            cout<<"-- db="<<db<<" d="<<d<<" N="<<N<<endl;
            Mat<double> zeta(T,K);
            Mat<double> phi(N,T);
            initZeta(zeta,lambda,x[d]);
            initPhi(phi,zeta,lambda,x[d]);

            //cout<<" ------------------------ doc level updates --------------------"<<endl;
            Mat<double> gamma(T,2);
            Mat<double> gamma_prev(T,2);
            gamma_prev.ones();
            gamma_prev.col(1) += mAlpha;
            bool converged = false;
            uint32_t o=0;
            while(!converged){
              //           cout<<"-------------- Iterating local params #"<<o<<" -------------------------"<<endl;
              updateGamma(gamma,phi);
              updateZeta(zeta,phi,a,lambda,x[d]);
              updatePhi(phi,zeta,gamma,lambda,x[d]);

              converged = (accu(gamma_prev != gamma))==0 || o>60 ;
              gamma_prev = gamma;
              ++o;
            }

            mZeta[d] = Mat<double>(zeta);
            mPhi[d] = Mat<double>(phi);
            mGamma[d] = Mat<double>(gamma);
            Mat<double> d_lambda(K,Nw);
            Mat<double> d_a(K,2); 
            //      cout<<" --------------------- natural gradients --------------------------- "<<endl;
            computeNaturalGradients(d_lambda, d_a, zeta, phi, mOmega, D, x[d]);
 #pragma omp critical
            {
              db_lambda += d_lambda;
              db_a += d_a;
            }
          }
          //for (uint32_t k=0; k<K; ++k)
          //  cout<<"delta lambda_"<<k<<" min="<<min(d_lambda.row(k))<<" max="<< max(d_lambda.row(k))<<" #greater 0.1="<<sum(d_lambda.row(k)>0.1)<<endl;
          // ----------------------- update global params -----------------------
          double bS = min(S,D-dd); // necessary for the last batch, which migth not form a complete batch
          double ro = exp(-kappa*log(1+double(dd)+double(bS)/2.0)); // as "time" use the middle of the batch 
          cout<<" -- global parameter updates dd="<<dd<<" bS="<<bS<<" ro="<<ro<<endl;
          //cout<<"d_a="<<d_a<<endl;
          //cout<<"a="<<a<<endl;
          lambda = (1.0-ro)*lambda + (ro/S)*db_lambda;
          a = (1.0-ro)*a + (ro/S)*db_a;
          mA=a;
          mLambda = lambda;

          mPerp[dd+bS/2] = 0.0;
          if (mX_ho.size() > 0) {
            cout<<"computing "<<mX_ho.size()<<" perplexities"<<endl;
#pragma omp parallel for schedule(dynamic) 
            for (uint32_t i=0; i<mX_ho.size(); ++i)
            {
              double perp_i =  perplexity(mX_ho[i],dd+bS/2+1,ro); //perplexity(mX_ho[i], mZeta[d], mPhi[d], mGamma[d], lambda);
              cout<<"perp_"<<i<<"="<<perp_i<<endl;
#pragma omp critical
              {
                mPerp[dd] += perp_i;
              }
            }
            mPerp[dd] /= double(mX_ho.size());
            //cout<<"Perplexity="<<mPerp[d]<<endl;
          }
        }
      }
      return z_dn; //TODO: return p_d(x)
    };

    // compute density estimate based on data previously fed into the class using addDoc
    bool densityEst(uint32_t Nw, double kappa, uint32_t K, uint32_t T, uint32_t S)
    {
      if(mX.size() > 0)
      {
        densityEst(mX,Nw,kappa,K,T,S);
        //TODO: return p_d(x)
        return true;
      }else{
        return false;
      }
    };

    // after an initial densitiy estimate has been made using densityEst()
    // can use this to update the estimate with information from additional x 
    bool  updateEst(const Mat<uint32_t>& x, double kappa)
    {
      if (mX.size() > 0 && mX.size() == mPhi.size()) { // this should indicate that there exists a estimate already
        uint32_t N = x.n_rows;
        uint32_t T = mT; //mZeta[0].n_rows;
        uint32_t K = mK; //mZeta[0].n_cols;
        mX.push_back(x);
        mZeta.push_back(Mat<double>(T,K));
        mPhi.push_back(Mat<double>(N,T));
        //    mZeta.set_size(T,K);
        //    mPhi.set_size(N,T);
        mGamma.push_back(Mat<double>(T,2));
        uint32_t d = mX.size()-1;
        mPerp.resize(d+1);

        if(updateEst(mX[d],mZeta[d],mPhi[d],mGamma[d],mA,mLambda,mOmega,d,kappa))
        {
          mPerp[d] = 0.0;
          for (uint32_t i=0; i<mX_ho.size(); ++i)
            mPerp[d] += perplexity(mX_ho[i], mZeta[d], mPhi[d], mGamma[d], mLambda);
          mPerp[d] /= double(mX_ho.size());
          cout<<"Perplexity="<<mPerp[d]<<endl;
          return true; 
        }else{
          return false;
        } 
      }else{
        return false;
      }
    };

    // compute the perplexity of a given document x
    double perplexity(const Mat<uint32_t>& x, uint32_t d, double kappa=0.75)
    {
      if (mX.size() > 0 && mX.size() == mPhi.size()) { // this should indicate that there exists a estimate already
        uint32_t N = x.n_rows;
        //uint32_t Nw = mLambda.n_cols;
        uint32_t T = mT; //mZeta[0].n_rows; 
        uint32_t K = mK; //mZeta[0].n_cols;

        Mat<double> zeta(T,K);
        Mat<double> phi(N,T);
        Mat<double> gamma(T,2);
        //uint32_t d = mX.size()-1;

        Mat<double> a(mA);// DONE: make deep copy here!
        Mat<double> lambda(mLambda);
        double omega = mOmega;

        //cout<<"updating copied model with x"<<endl;
        updateEst(x,zeta,phi,gamma,a,lambda,omega,d,kappa);
        //cout<<"computing perplexity under updated model"<<endl;

        return perplexity(x, zeta, phi, gamma, lambda);
      }else{
        return 1.0/0.0;
      }
    };

    // compute the perplexity given a document x and the model paremeters of it (after incorporating x)
    double perplexity(const Mat<uint32_t>& x, Mat<double>& zeta, Mat<double>& phi, Mat<double>& gamma, Mat<double>& lambda)
    {
      //cout<<"Computing Perplexity"<<endl;
      uint32_t N = x.n_rows;
      uint32_t T = mT; //mZeta[0].n_rows;
      // find most likely pi_di and c_di
      Col<double> pi;
      Col<double> sigPi; 
      Col<uint32_t> c(T);
      getDocTopics(pi, sigPi, c, gamma, zeta);
      // find most likely z_dn
      Col<uint32_t> z(N);
      getWordTopics(z, phi);
      // find most likely topics 
      Mat<double> topics;

      //cout<<" lambda.shape="<<lambda.n_rows<<" "<<lambda.n_cols<<endl;
      getCorpTopic(topics, lambda);

      double perp = 0.0;
      //cout<<"x: "<<x.n_rows<<"x"<<x.n_cols<<endl;
      for (uint32_t n=0; n<x.n_elem; ++n){
        //cout<<"c_z_n = "<<c[z[n]]<<" z_n="<<z[n]<<" n="<<n<<" N="<<x.n_rows<<" x_n="<<x[n]<<" topics.shape="<<topics.n_rows<<" "<<topics.n_cols<<endl;
        perp -= logCat(x[n],topics.row(c[z[n]]));
        //cout<<perp<<" ";
      } cout<<endl;
      perp /= double(x.n_elem);
      perp /= log(2.0); // since it is log base 2 in the perplexity formulation!
      perp = pow(2.0,perp);

      //        return logCat(self.x[d][n], self.beta[ self.c[d][ self.z[d][n]]]) \
      //    + logCat(self.c[d][ self.z[d][n]], self.sigV) \
      //    + logBeta(self.v, 1.0, self.omega) \
      //    + logCat(self.z[d][n], self.sigPi[d]) \
      //    + logBeta(self.pi[d], 1.0, self.alpha) \
      //    + logDir(self.beta[ self.c[d][ self.z[d][n]]], self.Lambda)
      //

      return perp;
    }


    bool updateEst(const Mat<uint32_t>& x, Mat<double>& zeta, Mat<double>& phi, Mat<double>& gamma, Mat<double>& a, Mat<double>& lambda, double omega, uint32_t d, double kappa= 0.75)
    {
      uint32_t D = d+1; // assume that doc d is appended to the end  
      //uint32_t N = x.n_rows;
      uint32_t Nw = lambda.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

      //    cout<<"---------------- Document "<<d<<" N="<<N<<" -------------------"<<endl;
      //    cout<<"a=\t"<<a.t();
      //    for (uint32_t k=0; k<K; ++k)
      //    {
      //      cout<<"@"<<k<<" lambda=\t"<<lambda.row(k);
      //    }

      initZeta(zeta,lambda,x);
      initPhi(phi,zeta,lambda,x);

      // ------------------------ doc level updates --------------------
      bool converged = false;
      Mat<double> gamma_prev(T,2);
      gamma_prev.ones();

      uint32_t o=0;
      while(!converged){
        //      cout<<"-------------- Iterating local params #"<<o<<" -------------------------"<<endl;
        updateGamma(gamma,phi);
        updateZeta(zeta,phi,a,lambda,x);
        updatePhi(phi,zeta,gamma,lambda,x);

        converged = (accu(gamma_prev != gamma))==0 || o>60 ;

        gamma_prev = gamma;
        //cout<<"zeta>"<<endl<<zeta<<"<zeta"<<endl;
        //cout<<"phi>"<<endl<<phi<<"<phi"<<endl;
        ++o;
      }

      //    cout<<" --------------------- natural gradients --------------------------- "<<endl;
      //    cout<<"\tD="<<D<<" omega="<<omega<<endl;

      Mat<double> d_lambda(K,Nw);
      Mat<double> d_a(K,2); 
      computeNaturalGradients(d_lambda, d_a, zeta, phi, omega, D, x);

      //    cout<<" ------------------- global parameter updates: ---------------"<<endl;
      //cout<<"delta a= "<<d_a.t()<<endl;
      //for (uint32_t k=0; k<K; ++k)
      //  cout<<"delta lambda_"<<k<<" min="<<min(d_lambda.row(k))<<" max="<< max(d_lambda.row(k))<<" #greater 0.1="<<sum(d_lambda.row(k)>0.1)<<endl;

      // ----------------------- update global params -----------------------
      double ro = exp(-kappa*log(1+double(d+1)));
      //    cout<<"\tro="<<ro<<endl;
      lambda = (1.0-ro)*lambda + ro*d_lambda;
      a = (1.0-ro)*a+ ro*d_a;

      return true;
    };

    void getA(Col<double>& a)
    {
      a=mA.col(0);
    };

    void getB(Col<double>& b)
    {
      b=mA.col(1);
    };

    bool getLambda(Col<double>& lambda, uint32_t k)
    {
      if(mLambda.n_rows > 0 && k < mLambda.n_rows)
      {
        lambda=mLambda.row(k).t();
        return true;
      }else{
        return false;
      }
    };

    bool getDocTopics(Col<double>& pi, Col<double>& sigPi, Col<uint32_t>& c, uint32_t d)
    {
      if (d < mGamma.size())
        return getDocTopics(pi,sigPi,c,mGamma[d],mZeta[d]);
      else{
        cout<<"asking for out of range doc "<<d<<" have only "<<mGamma.size()<<endl;
        return false;
      }
    };

    bool getDocTopics(Col<double>& pi, Col<double>& sigPi, Col<uint32_t>& c, const Mat<double>& gamma, const Mat<double>& zeta)
    {
      uint32_t T = gamma.n_rows; // doc level topics

      sigPi.set_size(T+1);
      pi.set_size(T);
      c.set_size(T);

      //cout<<"K="<<K<<" T="<<T<<endl;
      betaMode(pi,gamma.col(0),gamma.col(1));
      stickBreaking(sigPi,pi);
      //cout<<"pi="<<pi<<endl;
      //cout<<"sigPi="<<sigPi<<endl;
      //cout<<"mGamma="<<mGamma[d]<<endl;
      for (uint32_t i=0; i<T; ++i){
        c[i] = multinomialMode(zeta.row(i));
      }
      return true;
    };


    bool getWordTopics(Col<uint32_t>& z, uint32_t d){
      return getWordTopics(z,mPhi[d]);
    };

    bool getWordTopics(Col<uint32_t>& z, const Mat<double>& phi){
      z.set_size(phi.n_rows);
      for (uint32_t i=0; i<z.n_elem; ++i){
        z[i] = multinomialMode(phi.row(i));
      }
      return true;
    };

    bool getCorpTopicProportions(Col<double>& v, Col<double>& sigV)
    {
      return getCorpTopicProportions(v,sigV,mA);
    };

    // a are the parameters of the beta distribution from which v is drawn
    bool getCorpTopicProportions(Col<double>& v, Col<double>& sigV, const Mat<double>& a)
    {
      uint32_t K = a.n_rows; // corp level topics

      sigV.set_size(K+1);
      v.set_size(K);

      betaMode(v, a.col(0), a.col(1));
      stickBreaking(sigV,v);
      return true;
    };


    bool getCorpTopic(Col<double>& topic, uint32_t k)
    {
      if(mLambda.n_rows > 0 && k < mLambda.n_rows)
      {
        return getCorpTopic(topic,mLambda.row(k));
      }else{
        return false;
      }
    };

    bool getCorpTopic(Col<double>& topic, const Row<double>& lambda)
    {
      // mode of dirichlet (MAP estimate)
      dirMode(topic, lambda.t());
      return true;
    };

    bool getCorpTopic(Mat<double>& topics, const Mat<double>& lambda)
    {
      uint32_t K = lambda.n_rows;
      uint32_t Nw = lambda.n_cols;
      topics.set_size(K,Nw);
      for (uint32_t k=0; k<K; k++){
        // mode of dirichlet (MAP estimate)

        topics.row(k) = (lambda.row(k)-1.0)/sum(lambda.row(k)-1.0);
        //dirMode(topics.row(k), lambda.row(k));
      }
      return true;
    };

    Row<double> P_x(uint32_t d) const{
      Row<double> p(mNw);
      p.zeros();

      //Col<double> beta = dirMode(topic, lambda.t());

//      for (uint32_t w=0; w<mNw; ++w){
//        p[w] = logCat(w, beta[]
//      }
//
      return p;
    }

  protected:
    Mat<double> mLambda; // corpus level topics (Dirichlet)
    Mat<double> mA; // corpus level Beta process alpha parameter for stickbreaking
    vector<Mat<double> > mZeta; // document level topic indices/pointers to corpus level topics (Multinomial) 
    vector<Mat<double> > mPhi; // document level word to doc level topic assignment (Multinomial)
    vector<Mat<double> > mGamma; // document level Beta distribution alpha parameter for stickbreaking

    Col<double> mPerp; // perplexity for each document

    uint32_t mT; // Doc level truncation
    uint32_t mK; // Corp level truncation
    uint32_t mNw; // size of dictionary

};
