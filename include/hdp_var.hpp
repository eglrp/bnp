/* Copyright (c) 2012, Julian Straub <jstraub@csail.mit.edu>
 * Licensed under the MIT license. See LICENSE.txt or 
 * http://www.opensource.org/licenses/mit-license.php */

#pragma once

#include "hdp_base.hpp"
#include "hdp_var_base.hpp"

#include "random.hpp"
#include "baseMeasure.hpp"
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

/*
 * this one assumes that the number of words per document is 
 * smaller than the dictionary size
 *
 * http://en.wikipedia.org/wiki/Virtual_inheritance
 */
template <class U>
class HDP_var: public HDP<U>, public virtual HDP_var_base
{
  public:

    HDP_var(const BaseMeasure<U>& base, double alpha, double omega)
      : HDP_var_base(0,0,0), HDP<U>(base, alpha, omega)
    {};

    ~HDP_var()
    {};

    // interface mainly for python
    uint32_t addDoc(const Mat<U>& x_i)
    {
      uint32_t x_ind = HDP<U>::addDoc(x_i);
      // TODO: potentially slow
      // add the index of the added x_i
      mInd2Proc.resize(mInd2Proc.n_elem+1);
      mInd2Proc[mInd2Proc.n_elem-1] = x_ind;
      return x_ind;
    };

    /* 
     * Initializes the corpus level parameters mA and mLambda according 
     * to Blei's Stochastic Variational paper
     *
     * @param D is the assumed number of documents for init
     */
    void initCorpusParams(uint32_t Nw, uint32_t K, uint32_t T, uint32_t D)
    {

      mT = T;
      mK = K;
      mNw = Nw;

      mA.ones(K,2);
      mA.col(1) *= HDP<U>::mOmega; 

      // initialize lambda
      HDP<U>::mLambda.init(HDP<U>::mH0,K); // initialize the priors of the topics from the base measure.
    
//      HDP<U>::mLambda.zeros(K,Nw);
//      GammaRnd gammaRnd(1.0, 1.0);
//      for (uint32_t k=0; k<K; ++k){
//        for (uint32_t w=0; w<Nw; ++w) HDP<U>::mLambda(k,w) = gammaRnd.draw();
//        HDP<U>::mLambda.row(k) += ((Dir*)(&mH0))->mAlphas;
//      }
    };

    /* From: Online Variational Inference for the HDP
     * method for "one shot" computation without storing data in this class
     *  Nw: number of different words
     *  kappa=0.9: forgetting rate
     *  uint32_t T=10; // truncation on document level
     *  uint32_t K=100; // truncation on corpus level
     *  S = batch size
     */
    void densityEst(const vector<Mat<U> >& x, uint32_t Nw, 
        double kappa, uint32_t K, uint32_t T, uint32_t S)
    {
      cout<<"densityEstimate with: K="<<K<<"; T="<<T<<"; kappa="<<kappa<<"; Nw="<<Nw<<"; S="<<S<<endl;

      HDP<U>::mX = x;
      uint32_t D=HDP<U>::mX.size();
      cout<<"D="<<D<<endl;
      cout<<"mX[0].shape= "<<HDP<U>::mX[0].n_rows<<"x"<<HDP<U>::mX[0].n_cols<<endl;

      mInd2Proc = linspace<Row<uint32_t> >(0,D-1,D);

      mT = T;
      mK = K;
      mNw = Nw;

      initCorpusParams(mNw,mK,mT,D);
      cout<<"Init of corpus params done"<<endl;
      Row<uint32_t> ind = updateEst_batch(mInd2Proc,mZeta,mPhi,mGamma,mA,HDP<U>::mLambda,mPerp,HDP<U>::mOmega,kappa,S,true);
//      cout<<"mPhi -> D="<<mPhi.size()<<endl;
//      cout<<"mPhi -> D="<<HDP_var_base::mPhi.size()<<endl;
//      cout<<"mPerp="<<mPerp.t()<<endl;

      Mat<double> pi(D,T);
      Mat<double> sigPi(D,T+1);
      Mat<uint32_t> c(D,T);
      getDocTopics(pi,sigPi,c);
//      cout<<"c:"<<c<<endl;

      mInd2Proc.set_size(0); // all processed

    };

    /*
     * compute density estimate based on data previously fed into the class using addDoc
     */
    bool densityEst(uint32_t Nw, double kappa, uint32_t K, uint32_t T, uint32_t S)
    {
      if(HDP<U>::mX.size() > 0)
      {
        densityEst(HDP<U>::mX,Nw,kappa,K,T,S);
        //TODO: return p_d(x)
        return true;
      }else{
        return false;
      }
    };


    /* 
     * updated the estimate using newly added docs in mX
     * "newly added": the ones that are indicated in mInd2Proc
     */
    bool updateEst_batch(double kappa, uint32_t S)
    {
      uint32_t Db=mInd2Proc.n_elem;
      if (Db >0){  
        cout<<"updatedEstimate with: K="<<mK<<"; T="<<mT<<"; kappa="<<kappa<<"; Nw="<<mNw<<"; S="<<S<<endl;
        vector<Mat<double> > zeta; // will get resized accordingly inside updateEst_batch
        vector<Mat<double> > phi;
        vector<Mat<double> > gamma;
        Col<double> perp;

        Row<uint32_t> ind = updateEst_batch(mInd2Proc,zeta,phi,gamma,mA,HDP<U>::mLambda,perp,HDP<U>::mOmega,kappa,S);

        mZeta.resize(mZeta.size()+Db);
        mPhi.resize(mPhi.size()+Db);
        mGamma.resize(mGamma.size()+Db);
        mPerp.resize(mPerp.n_elem+Db);
        mPerp.rows(mPerp.n_elem-Db,mPerp.n_elem-1) = perp;
        for (uint32_t i=0; i<ind.n_elem; ++i){
          mZeta[ind[i]] = zeta[i];
          mPhi[ind[i]] = phi[i];
          mGamma[ind[i]] = gamma[i];
        }

        mInd2Proc.set_size(0); // all processed

        return true;
      }else{
        cout<<"add more documents before starting to process"<<endl;
      }
      return false;
    };

    /*
     * after an initial densitiy estimate has been made using densityEst()
     * can use this to update the estimate with information from additional x 
     */
    bool  updateEst(const Mat<U>& x, double kappa)
    {
      if (HDP<U>::mX.size() > 0 && HDP<U>::mX.size() == mPhi.size()) { // this should indicate that there exists an estimate already
        uint32_t N = x.n_cols;
        uint32_t T = mT; 
        uint32_t K = mK;
        HDP<U>::mX.push_back(x);
        mZeta.push_back(Mat<double>(T,K));
        mPhi.push_back(Mat<double>(N,T));
        //    mZeta.set_size(T,K);
        //    mPhi.set_size(N,T);
        mGamma.push_back(Mat<double>(T,2));
        uint32_t d = HDP<U>::mX.size()-1;
        mPerp.resize(d+1);


        if(updateEst(HDP<U>::mX[d],mZeta[d],mPhi[d],mGamma[d],mA,HDP<U>::mLambda,HDP<U>::mOmega,d,kappa))
        {
          mPerp[d] = 0.0;
          for (uint32_t i=0; i < HDP<U>::mX_ho.size(); ++i)
          {    
            Row<double> logP=logP_w(HDP<U>::mX[d],mPhi[d], mZeta[d], mGamma[d], HDP<U>::mLambda);
            mPerp[d] += HDP<U>::perplexity(HDP<U>::mX_ho[i], logP);
            //mPerp[d] += perplexity(mX_ho[i], mZeta[d], mPhi[d], mGamma[d], HDP<U>::mLambda);
          }
          mPerp[d] /= double(HDP<U>::mX_ho.size());
          cout<<"Perplexity="<<mPerp[d]<<endl;
          return true; 
        }else{
          return false;
        } 
      }else{
        return false;
      }
    };

    /*
     *
     */
    bool updateEst(const Mat<U>& x, Mat<double>& zeta, Mat<double>& phi, Mat<double>& gamma, Mat<double>& a, DistriContainer<U>& lambda, double omega, uint32_t d, double kappa)
    {
      uint32_t D = d+1; // assume that doc d is appended to the end  
//      uint32_t Nw = lambda.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

      Mat<double> eLogBeta(mK,x.n_cols); //TODO
      Col<double> digam_lamb_sum(mK);

      compElogBeta(eLogBeta,  lambda, x);

      Col<double> eLogSig_a(K);
      compElogSig(eLogSig_a, a);

      //    cout<<"---------------- Document "<<d<<" N="<<N<<" -------------------"<<endl;
      initZeta(zeta,eLogBeta);
      initPhi(phi,zeta,eLogBeta);

      if(!is_finite(zeta))
      {
        cout<<"updateEst::a="<<size(a)<<endl<<a;
        cout<<"updateEst::lambda="<<lambda[0]->asRow();
        cout<<"updateEst::x="<<size(x)<<endl<<x;
        cout<<"zeta_init="<<zeta<<endl;
        cout<<"phi_init="<<phi<<endl;
        cout<<"eLogBeta="<<eLogBeta<<endl;
        exit(0);
      }

      // ------------------------ doc level updates --------------------
      bool converged = false;
      Col<double> eLogSig_gam(T);
      Mat<double> gamma_prev(T,2);
      gamma_prev.ones();

      uint32_t o=0;
      while(!converged){
        //      cout<<"-------------- Iterating local params #"<<o<<" -------------------------"<<endl;
        updateGamma(gamma,phi);

        if (!is_finite(gamma)){
          cout<<"gamma="<<gamma;
          cout<<"phi="<<phi;
          exit(1);
        }


        compElogSig(eLogSig_gam,gamma); // precompute 

        updateZeta(zeta,phi,eLogSig_a,eLogBeta);
        updatePhi(phi,zeta,eLogSig_gam,eLogBeta);

        converged = (accu(gamma_prev != gamma))==0 || o>60 ;
        gamma_prev = gamma;
        ++o;

        if(!is_finite(zeta))
        {
          cout<<"o="<<o<<endl;
          cout<<"zeta="<<zeta.row(0)<<" |.|="<<sum(exp(zeta.row(0)))<<endl;
          cout<<"phi="<<phi.row(0)<<" |.|="<<sum(exp(phi.row(0)))<<endl;
          exit(0);
        }
      }

      //    cout<<" --------------------- natural gradients --------------------------- "<<endl;
      //    cout<<"\tD="<<D<<" omega="<<omega<<endl;
      DistriContainer<U> d_lambda(HDP<U>::mH0,K);
      Mat<double> d_a(K,2); 
      computeNaturalGradients(d_lambda, d_a, zeta, phi, omega, D, x);

      cout<<"update::d_lambda:"<<d_lambda.toMat().rows(0,5);
      cout<<"update::lambda:"<<lambda.toMat().rows(0,5);

      //    cout<<" ------------------- global parameter updates: ---------------"<<endl;
      double ro = exp(-kappa*log(1+double(d+1)));
      //    cout<<"\tro="<<ro<<endl;
      for (uint32_t k=0; k<mK; ++k)
        lambda[k]->fromRow( (1.0-ro)*lambda[k]->asRow() + ro* d_lambda[k]->asRow());

      //lambda = (1.0-ro)*lambda + ro*d_lambda;
      a = (1.0-ro)*a+ ro*d_a;
      return true;
    };


    /*
     * Updates the estimate using mini batches
     * @param ind_x indices of docs to process within docs mX. !these are assumed to be in order!
     * @param sameIndAsX == true -> zeta,phi,gamma have same indices as x (ind_x). This typically happens for the initial batch update. Setting this to true eliminates the need of reordering the results afterwords to match the indices of the docs x.
     * @return the randomly shuffled indices to show how the data was processed -> this allows association of zetas, phis and gammas with docs in mX
     */
    Row<uint32_t> updateEst_batch(const Row<uint32_t>& ind_x, vector<Mat<double> >& zeta, vector<Mat<double> >& phi, vector<Mat<double> >& gamma, Mat<double>& a, DistriContainer<U>& lambda, Col<double>& perp, double omega, double kappa, uint32_t S, bool sameIndAsX=false)
    {
      uint32_t d_0 = min(ind_x); // thats the doc number that we start with -> needed for ro computation; assumes that all indices in mX prior to d_0 have already been processed.
      uint32_t D= max(ind_x)+1; // D is the maximal index of docs that we are processing +1

      Row<uint32_t> ind = shuffle(ind_x,1);
//        cout<<"ind_x: "<<ind_x.cols(0,S)<<endl;
//        cout<<"ind  : "<<ind.cols(0,S)<<endl;

      zeta.resize(ind.n_elem,Mat<double>(mT,mK));
      phi.resize(ind.n_elem);
      gamma.resize(ind.n_elem,Mat<double>(mT,2));
      perp.zeros(ind.n_elem);

      for (uint32_t dd=0; dd<ind.n_elem; dd += S)
      {
        DistriContainer<U> db_lambda(HDP<U>::mH0,mK);

        Mat<double> db_a(mK,2); 
        db_a.zeros();

        Col<double> eLogSig_a(mK);
        compElogSig(eLogSig_a, a);

#pragma omp parallel for schedule(dynamic) 
        for (uint32_t db=dd; db<min(dd+S,ind.n_elem); db++)
        {
          uint32_t d=ind[db];  
          const Mat<U>& x_d = HDP<U>::mX[d];
          uint32_t dout=sameIndAsX?d:db;
          uint32_t N=x_d.n_cols;
          //      cout<<"---------------- Document "<<d<<" N="<<N<<" -------------------"<<endl;

          cout<<"-- db="<<db<<" d="<<d<<" N="<<N<<endl;

          Mat<double> eLogBeta(mK,x_d.n_cols);
          Col<double> digam_lamb_sum(mK);
          compElogBeta(eLogBeta, lambda, x_d);

          //Mat<double> zeta(T,K);
          phi[dout].resize(N,mT);
          initZeta(zeta[dout],eLogBeta);
          initPhi(phi[dout],zeta[dout],eLogBeta);

//            cout<<"zeta_init="<<zeta[dout]<<endl;
//            cout<<"phi_init="<<phi[dout]<<endl;
//            cout<<"eLogBeta="<<eLogBeta<<endl;
//            if(!is_finite(zeta[dout]))
//              exit(0);

          //cout<<" ------------------------ doc level updates --------------------"<<endl;
          //Mat<double> gamma(T,2);
          Col<double> eLogSig_gam(mT);
          Mat<double> gamma_prev(mT,2);
          gamma_prev.ones();
          gamma_prev.col(1) += HDP<U>::mAlpha;
          bool converged = false;
          uint32_t o=0;
          while(!converged){
//            cout<<"-------------- Iterating local params #"<<o<<" -------------------------"<<endl;
            updateGamma(gamma[dout],phi[dout]);

            if (!is_finite(gamma[dout])){
              cout<<"gamma="<<gamma[dout];
              cout<<"phi="<<phi[dout];
              exit(1);
            }

            compElogSig(eLogSig_gam,gamma[dout]); // precompute 

            updateZeta(zeta[dout],phi[dout],eLogSig_a,eLogBeta);
            updatePhi(phi[dout],zeta[dout],eLogSig_gam,eLogBeta);

            converged = (accu(gamma_prev != gamma[dout]))==0 || o>30 ;
            gamma_prev = gamma[dout];
            ++o;

//            cout<<"o="<<o<<endl;
//            cout<<"zeta="<<zeta[dout].row(0)<<" |.|="<<sum(zeta[dout].row(0))<<endl;
//            cout<<"phi="<<phi[dout].row(0)<<" |.|="<<sum(phi[dout].row(0))<<endl;
//            if(!is_finite(zeta[dout]))
//              exit(0);
          }

          DistriContainer<U> d_lambda(HDP<U>::mH0,mK); // batch updates
          Mat<double> d_a(mK,2); 
//          cout<<" --------------------- natural gradients dout="<< dout<<" dd="<< dd<<" --------------------------- "<<endl;
          computeNaturalGradients(d_lambda, d_a, zeta[dout], phi[dout], HDP<U>::mOmega, D, x_d);
#pragma omp critical
          {
            cout<<"update_batch::d_lambda:"<<endl<<d_lambda.toMat().rows(0,5);
            for (uint32_t k=0; k<d_lambda.size(); ++k){
              db_lambda[k]->fromRow(db_lambda[k]->asRow() + d_lambda[k]->asRow());
            }
            db_a += d_a;
          }
        }
        //for (uint32_t k=0; k<K; ++k)
        //  cout<<"delta lambda_"<<k<<" min="<<min(d_lambda.row(k))<<" max="<< max(d_lambda.row(k))<<" #greater 0.1="<<sum(d_lambda.row(k)>0.1)<<endl;
        // ----------------------- update global params -----------------------
        uint32_t t=dd+d_0; // d_0 is the timestep of the the first index to process; dd is the index in the current batch
        uint32_t bS = min(S,ind.n_elem-dd); // necessary for the last batch, which migth not form a complete batch
        //TODO: what is the time dd? d_0 needed?
        double ro = exp(-kappa*log(1+double(t)+double(bS)/2.0)); // as "time" use the middle of the batch 
//        cout<<" -- global parameter updates t="<<t<<" bS="<<bS<<" ro="<<ro<<endl;
//        cout<<"d_a="<<db_a<<endl;
        

//        cout<<"dLambda"<<endl;
//        for (uint32_t k=0; k<10; ++k)
//          cout<<db_lambda[k]->asRow();
//        
//        cout<<"Before"<<endl;
//        for (uint32_t k=0; k<10; ++k)
//          cout<<lambda[k]->asRow();

        cout<<"update_batch::db_lambda:"<<endl<<db_lambda.toMat().rows(0,5);
        for (uint32_t k=0; k<db_lambda.size(); ++k)
          lambda[k]->fromRow((1.0-ro)*lambda[k]->asRow() + (ro/S)*db_lambda[k]->asRow()); //TODO: doies this make sense for NIW prior???
        cout<<"update_batch::lambda(after):"<<endl<<lambda.toMat().rows(0,5);

//        cout<<"After"<<endl;
//        for (uint32_t k=0; k<10; ++k)
//          cout<<lambda[k]->asRow();


        //lambda = (1.0-ro)*lambda + (ro/S)*db_lambda;
        a = (1.0-ro)*a + (ro/S)*db_a;
        cout<<"update_batch::lambda:"<<lambda.toMat().rows(0,5);

        perp[dd+bS/2] = 0.0;
        if (HDP<U>::mX_te.size() > 0) {
          cout<<"computing "<<HDP<U>::mX_te.size()<<" perplexities"<<endl;
#pragma omp parallel for schedule(dynamic) 
          for (uint32_t i=0; i < HDP<U>::mX_te.size(); ++i)
          {
            //cout<<"mX_te: "<< mX_te[i].n_rows << "x"<< mX_te[i].n_cols<<endl;
            //cout<<"mX_ho: "<< mX_ho[i].n_rows << "x"<< mX_ho[i].n_cols<<endl;
            //TODO: these subfunctions work on the member variables!!! dont do that...
            double perp_i =  perplexity(HDP<U>::mX_te[i],HDP<U>::mX_ho[i],dd+bS/2+1,ro); //perplexity(mX_ho[i], mZeta[d], mPhi[d], mGamma[d], lambda);
            //cout<<"perp_"<<i<<"="<<perp_i<<endl;
#pragma omp critical
            {
              perp[dd+bS/2] += perp_i;
            }
          }
          perp[dd+bS/2] /= double(HDP<U>::mX_te.size());
          cout<<"Perplexity="<<perp[dd+bS/2]<<endl;
        }
      }
      cout<<"perp="<<perp.t()<<endl;
      return ind;
    };


    // compute the perplexity of a given document split into x_test (to find a topic model for the doc) and x_ho (to evaluate the perplexity)
    double perplexity(const Mat<U>& x_te, const Mat<U>& x_ho, uint32_t d, double kappa=0.75)
    {
      if (HDP<U>::mX.size() > 0 && HDP<U>::mX.size() == mPhi.size()) { // this should indicate that there exists a estimate already
       
        uint32_t N = x_te.n_cols;
        uint32_t T = mT; 
        uint32_t K = mK; 

        Mat<double> zeta(T,K);
        Mat<double> phi(N,T);
        Mat<double> gamma(T,2);
        //uint32_t d = mX.size()-1;

        Mat<double> a(mA);// DONE: make deep copy here!

        DistriContainer<U> lambda(HDP<U>::mLambda);
        double omega = HDP<U>::mOmega;


        cout<<"perplexity::lambda:"<<lambda.toMat().rows(0,5);
        cout<<"perplexity::mLambda:"<<HDP<U>::mLambda.toMat().rows(0,5);

        cout<<"updating copied model with x"<<endl;
        updateEst(x_te,zeta,phi,gamma,a,lambda,omega,d,kappa);
//        cout<<" lambda.shape="<<lambda.size()<<endl;
        cout<<"computing perplexity under updated model"<<endl;
        //TODO: compute probabilities then use that to compute perplexity

        cout<<"x_te: "<<size(x_te);
        Mixture<U> mix = docMixture(phi, zeta, gamma, lambda);
        return HDP<U>::perplexity(x_ho, mix);
        //return perplexity(x_ho, zeta, phi, gamma, lambda);
      }else{
        return 1.0/0.0;
      }
    };

    Mixture<U> docMixture(uint32_t d) const {
      return docMixture(mPhi[d],mZeta[d],mGamma[d],HDP<U>::mLambda);
    };
    Mixture<U> docMixture(const Mat<double>& phi, const Mat<double>& zeta, const Mat<double>& gamma, const DistriContainer<U>& lambda) const
    {
      Col<double> pi;
      Col<double> sigPi;
      Col<uint32_t> c;
      getDocTopics(pi,sigPi,c,gamma,zeta);
      //cout<<"getDocTopics done"<<endl;
      //cout<<"c="<<c.t()<<size(c);
      Col<uint32_t> z(mNw);
      getWordTopics(z, phi);
      //cout<<"getWordTopics done"<<endl;
      //cout<<"z="<<z.t()<<size(z);

      DistriContainer<U> beta;
      HDP<U>::getCorpTopics(beta,lambda);
//      cout<<"getCorpTopics done"<<endl;
      //cout<<"beta:\t"<<size(beta);

      Col<uint32_t> c_u = unique(c);
      Row<double> ps(c_u.n_elem); // proportions in  the mixture
      DistriContainer<U> beta_d(c_u.n_elem);
      for (uint32_t i=0; i< c_u.n_elem; ++i){
        beta_d[i] = beta[c_u(i)]->getCopy();
        ps[i] = sum(sigPi.elem(find(c == c_u(i) )));
      }
//      cout<<"Mixture done"<<endl;
      return Mixture<U>(beta_d,ps);
    };

    /* Probability distribution over the words in document d
     *
     * TODO: so is that here not some MAP or ML estimate?!
     */
    Row<double> logP_w(uint32_t d) const {
//      cout<<"mX.size="<<mX.size()<<endl;
      return logP_w(HDP<U>::mX[d],mPhi[d],mZeta[d],mGamma[d],HDP<U>::mLambda);
    };
    /* 
     * log probability using the samples x (not using sufficient statistics -> x is just a list of words)
     */
    Row<double> logP_w(const Mat<U>& x, const Mat<double>& phi, const Mat<double>& zeta, const Mat<double>& gamma, const DistriContainer<U>& lambda) const
    { 
      Row<double> p(x.n_cols);
      p.zeros();
//      cout<<"x:\t"<<size(x);
//      cout<<"phi:\t"<<size(phi);
//      cout<<"zeta:\t"<<size(zeta);
//      cout<<"gamma:\t"<<size(gamma);
//      cout<<"lambda:\t"<<size(lambda);

      Col<double> pi;
      Col<double> sigPi;
      Col<uint32_t> c;
      getDocTopics(pi,sigPi,c,gamma,zeta);
      //cout<<"getDocTopics done"<<endl;
      //cout<<"c="<<c.t()<<size(c);
      Col<uint32_t> z(mNw);
      getWordTopics(z, phi);
      //cout<<"getWordTopics done"<<endl;
      //cout<<"z="<<z.t()<<size(z);

      DistriContainer<U> beta;
      HDP<U>::getCorpTopics(beta,lambda);
      //cout<<"getCorpTopics done"<<endl;
      //cout<<"beta:\t"<<size(beta);

      for (uint32_t i=0; i<x.n_cols; ++i){
        //cout<<"z_"<<i<<"="<<z[i]<<endl;
        p[i] = beta[c[z[i]]]->logP(x.col(i));// logCat(x[i], beta.row( c[ z[i] ]));
        //cout<<"p_"<<x[i]<<"="<<p[x[i]]<<endl;
      }
//      for (uint32_t w=0; w<mNw; ++w)
//        p[w] = p[w]==0.0?-1e10:p[w];

      //cout<<"p="<<p<<endl;
      return p;
    };

  protected:

    Row<uint32_t> mInd2Proc; // indices of docs that have not been processed

  private:

    /*
     * precompute necessary digamma function values, because these are slowing the whole algorithm down
     * all the update methods for zeta and phi need these values very often! I can precumpute these once after updating the global parameters (and hence lambda)
     */
    void compElogBeta(Mat<double>& eLogBeta, const DistriContainer<U>& lambda, const Mat<U>& x_d) const 
    { 
      eLogBeta.zeros(mK,x_d.n_cols);

      Col<double> digam_lamb_sum(mK);
    //  Mat<uint32_t> x_u = unique(x_d); // cannot do this trick anymore since x_d are continuous for the NIW base measure
//      cout<<"x_d="<<x_d<<endl;
//      cout<<"x_u="<<x_u<<endl;
 
      for (uint32_t i = 0; i < x_d.n_cols ; i++) {
        for (uint32_t k = 0; k < mK; k++) {
           eLogBeta(k,i) = lambda[k]->Elog(x_d.col(i)); // E[log beta] computation in paper
        }
      }
//      for (uint32_t k = 0; k < mK; k++) {
//        digam_lamb_sum(k) = digamma(sum(lambda.row(k)));
//      }
//      for (uint32_t i = 0; i < x_u.n_elem ; i++) {
//        for (uint32_t k = 0; k < mK; k++) {
//           eLogBeta(k,x_u(i)) = digamma(lambda(k,x_u(i))) - digam_lamb_sum(k);
//        }
//      }
    }

    void compElogSig(Col<double>& eLogSig, const Mat<double>& a) const
    {
      for (uint32_t k=0; k<a.n_rows; ++k){
        eLogSig(k) = digamma(a(k,0)) - digamma(a(k,0) + a(k,1));
        for (uint32_t l=0; l<k; ++l)
          eLogSig(k) += digamma(a(l,1)) - digamma(a(l,0) + a(l,1));
      }
    }

    void initZeta(Mat<double>& zeta, const Mat<double>& eLogBeta)
    {
      uint32_t N = eLogBeta.n_cols; // x_d.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;
      //cerr<<"\tinit zeta"<<endl;
      for (uint32_t i=0; i<T; ++i) {
        for (uint32_t k=0; k<K; ++k) {
          zeta(i,k)=0.0;
          for (uint32_t n=0; n<N; ++n) {
            //if(i==0 && k==0) cout<<zeta(i,k)<<" -> ";
            zeta(i,k) += eLogBeta(k,n); //ElogBeta(lambda, k, x_d(n));
          }
        }
        normalizeLogDistribution(zeta.row(i));
        //cout<<" normalized="<<zeta(0,0)<<endl;
      }
      //cerr<<"zeta>"<<endl<<zeta<<"<zeta"<<endl;
      //cerr<<"normalization check:"<<endl<<sum(zeta,1).t()<<endl; // sum over rows
    };

    void initPhi(Mat<double>& phi, const Mat<double>& zeta, const Mat<double>& eLogBeta)
    {
      uint32_t N = phi.n_rows; // x_d.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;
      //cout<<"\tinit phi"<<endl;
      for (uint32_t n=0; n<N; ++n){
        for (uint32_t i=0; i<T; ++i) {
          phi(n,i)=0.0;
          for (uint32_t k=0; k<K; ++k) {
            phi(n,i)+=zeta(i,k)* eLogBeta(k,n); // ElogBeta(lambda, k, x_d(n));
          }
        }
        normalizeLogDistribution(phi.row(n));
      }
      //cerr<<"phi>"<<endl<<phi<<"<phi"<<endl;
    };

    void updateGamma(Mat<double>& gamma, const Mat<double>& phi)
    {
      uint32_t N = phi.n_rows;
      uint32_t T = phi.n_cols;

      gamma.ones();
      gamma.col(1) *= HDP<U>::mAlpha;
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

    void updateZeta(Mat<double>& zeta, const Mat<double>& phi, const Col<double>& eLogSig_a, const Mat<double>& eLogBeta)
    {
//      assert(x_d.n_rows == 1);

      uint32_t N = phi.n_rows; // x_d.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

      for (uint32_t i=0; i<T; ++i){
        //zeta(i,k)=0.0;
        for (uint32_t k=0; k<K; ++k) {
          zeta(i,k) = eLogSig_a(k); //ElogSigma(a,k);
          //cout<<zeta(i,k)<<endl;
          for (uint32_t n=0; n<N; ++n){
            zeta(i,k) += phi(n,i)* eLogBeta(k,n); //ElogBeta(lambda,k,x_d(n));
          }
        }
        normalizeLogDistribution(zeta.row(i));
      }
    }


    void updatePhi(Mat<double>& phi, const Mat<double>& zeta, const Col<double>& eLogSig_gam, const Mat<double>& eLogBeta)
    {
//      assert(x_d.n_rows == 1);

      uint32_t N = phi.n_rows; // x_d.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

      for (uint32_t n=0; n<N; ++n){
        //phi(n,i)=0.0;
        for (uint32_t i=0; i<T; ++i) {
          phi(n,i) = eLogSig_gam(i); //ElogSigma(gamma,i);
          for (uint32_t k=0; k<K; ++k) {
            phi(n,i) += zeta(i,k)* eLogBeta(k,n); //ElogBeta(lambda,k,x_d(n)) ;
          }
        }
        normalizeLogDistribution(phi.row(n));
      }
    }

    void computeNaturalGradients(DistriContainer<U>& d_lambda, Mat<double>& d_a, const Mat<double>& zeta, const Mat<double>&  phi, double omega, uint32_t D, const Mat<U>& x_d)
    {
//      uint32_t N = x_d.n_cols;
//      uint32_t Nw = d_lambda.n_cols;
      uint32_t T = zeta.n_rows;
      uint32_t K = zeta.n_cols;

//      d_lambda.init(HDP<U>::mH0,mK);

//      d_lambda.zeros();
      d_a.zeros();
      for (uint32_t k=0; k<K; ++k) 
      { // for all K corpus level topics
//        cout<<zeta.col(k).t()<<endl;
//        cout<<phi;
        d_lambda[k]->posteriorHDP_var(zeta.col(k),phi,D,x_d);

//        for (uint32_t i=0; i<T; ++i) 
//        {
//          Row<double> _lambda(Nw); _lambda.zeros();
//          for (uint32_t n=0; n<N; ++n){
//            _lambda(x_d(n)) += phi(n,i);
//          }
//          d_lambda.row(k) += zeta(i,k) * _lambda;
//        }
//        d_lambda.row(k) = D*d_lambda.row(k);
//        //cout<<"lambda-nu="<<d_lambda[k].t()<<endl;
//        d_lambda.row(k) += ((Dir*)(&mH0))->mAlphas;
//        //cout<<"lambda="<<d_lambda[k].t()<<endl;

        for (uint32_t i=0; i<T; ++i) 
        {
          d_a(k,0) += zeta(i,k);
          for (uint32_t l=k+1; l<K; ++l) {
            d_a(k,1) += zeta(i,l);
          }
        }
        d_a(k,0) = D*d_a(k,0)+1.0;
        d_a(k,1) = D*d_a(k,1)+omega;
      }
    }


    //bool normalizeLogDistribution(Row<double>& r)
    bool normalizeLogDistribution(arma::subview_row<double> r) const
    {
      // known as the log sum exp trick!
//      cout<<" r="<<r<<endl;
      double maxR = as_scalar(max(r));
//      cout<<"  maxR="<<maxR<<endl;
//      cout<<"  exp(r-maxR)="<<exp(r-maxR)<<endl;
     
      r -= maxR + log(sum(exp(r-maxR)));
      r=exp(r);

//      cout<<" r="<<r<<endl;
      return true;


//      double minR = as_scalar(min(r));
//      cout<<" minR="<<minR<<endl;
//      if(minR > -100.0) {
//        cout<<" logDenom="<<sum(exp(r),1)<<endl;
//        double denom = as_scalar(sum(exp(r),1));
//        cout<<" logDenom="<<denom<<endl;
//        r -= log(denom); // avoid division by 0
//        cout<<" r - logDenom="<<r<<endl;
//        r = exp(r);
//        r /= sum(r);
//        cout<<" exp(r - logDenom)="<<r<<endl;
//        return true;
//      }else{ // cannot compute this -> set the smallest r to 1.0 and the rest to 0
//        double maxR = as_scalar(max(r));
//        cout<<"maxR="<<maxR<<" <-" <<arma::max(r) <<endl;
//        uint32_t kMax=as_scalar(find(r==maxR,1));
//        cout<<"maxR="<<maxR<<" kMax="<<kMax<<" <-" <<arma::max(r) <<endl;
//        r.zeros();
//        r(kMax)=1.0;
//        cout<<" r ="<<r<<endl;
//        return false;
//      }
    }

};

