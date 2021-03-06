#include "Math.h"
#include "../Log.h"

static const std::string ERROR_SOURCE = "STATISTICS_HELPER";

int maxValue(double zero, double one, double two){

    if(zero > one){
        if (zero > two)
            return 0;
        else if(two > zero)
            return 2;
    }
    else{
        if (one > two)
            return 1;
        else if(two > one)
            return 2;
    }

    return -1;
}

/*
Approximates the p-value from the pdf of the normal distribution where x is a Z-score

@param x Z-score.
@return p-value.
*/

double pnorm(double x) // Phi(-∞, x) aka N(x)
{
    return 0.5 * std::erfc(-x/std::sqrt(2));
}
double pnorm2(double x){
    // constants
    double a1 = 0.254829592;
    double a2 = -0.284496736;
    double a3 = 1.421413741;
    double a4 = -1.453152027;
    double a5 = 1.061405429;
    double p = 0.3275911;

    x = fabs(x) / sqrt(2.0);

    // A&S formula 7.1.26
    double t = 1.0 / (1.0 + p*x);

    return (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t*exp(-x*x);
}

/*
Finds p-value for test statistic using a chi-squared distribution with one degree of freedom
using chi-squared probability density function.

@param statistic Test statistic.
@return p-value.
*/
double chiSquareOneDOF(double statistic) {
    double z = statistic * 0.5;
    double sc = 2 * sqrt(z) * exp(-z);

    double sum = 1;
    double prevSum = sum;
    double nom = 1;
    double dnom = 1;
    double s = 0.5;

    for (int i = 0; i < 200; i++)
    {
        nom *= z;
        s++;
        dnom *= s;
        sum += nom / dnom;
        if (prevSum == sum) break;
        prevSum = sum;
    }

    double minVal = 1e-14;

    double p = sum * sc;
    if (std::isnan(p) || p < 0)
        return NAN;

    if(std::isinf(p))
        return minVal;

    p /= tgamma(0.5);

    p = std::max(p, 0.0);
    return std::max(1 - p, minVal);
}

//same as doing pairwise.complete.obs in R
MatrixXd covariance(MatrixXd &M) {
    MatrixXd centered = M.rowwise() - M.colwise().mean();
    MatrixXd cov = (centered.adjoint() * centered) / double(M.rows());
	return cov;
}

MatrixXd correlation(MatrixXd &M) {
    if(M.cols() == 1)
        return MatrixXd::Constant(1,1,1);

    MatrixXd centered = M.rowwise() - M.colwise().mean();
    MatrixXd cov = (centered.adjoint() * centered);
    VectorXd v = centered.array().pow(2).colwise().sum();
    MatrixXd var = v * v.transpose();

    MatrixXd cor = cov.array()/var.array().sqrt();
    return cor;
}

MatrixXd calculateHatMatrix(MatrixXd &Z){
    try{
        return Z*(Z.transpose()*Z).inverse()*Z.transpose();
    }catch(...){
        throwError(ERROR_SOURCE, "Error while calculate covariate hat matrix. Not invertable?");
    }
    MatrixXd null; return null;
}

MatrixXd calculateHatMatrix(MatrixXd &Z, MatrixXd &W, MatrixXd &sqrtW){
    try{
        return sqrtW*Z*(Z.transpose()*W*Z).inverse()*Z.transpose()*sqrtW;
    }catch(...){
        throwError(ERROR_SOURCE, "Error while calculate covariate hat matrix. Not invertable?");
    }
    MatrixXd null; return null;
}

VectorXd getBeta(VectorXd &Y, MatrixXd &Z, Family family) {

    if(family == Family::BINOMIAL)
        return CovariateBinomialRegression(Y, Z);
    if(family == Family::NORMAL)
        return CovariateNormalRegression(Y, Z);

    throwError(ERROR_SOURCE, "Family not recognized, could not compute beta values.");
}

VectorXd fitModel(VectorXd &beta, MatrixXd &Z, Family family) {

    VectorXd meanValue = Z * beta;

    if(family == Family::BINOMIAL)
        meanValue = 1 / (1 + exp(-meanValue.array()));

    //if(family == Family::NORMAL)
        //do nothing

    return meanValue;

}

VectorXd logisticRegression(VectorXd &Y, MatrixXd &X) {

    VectorXd beta = VectorXd::Constant(X.cols(), 0);
    VectorXd lastBeta = VectorXd::Constant(X.cols(), 0);

    int nobs = X.rows();

    std::vector<VectorXd> x;
    std::vector<MatrixXd> xxT;

    for(int i = 0; i< nobs; i++){
        x.push_back(X.row(i));
        xxT.push_back(X.row(i)*X.row(i).transpose());
    }

    VectorXd first;
    MatrixXd hess;

    int iteration = 0;

    while (true) {

        iteration++;

        VectorXd p(nobs);
        VectorXd W(nobs);

        for(int i =0; i < nobs; i++){
            double s = sigmoid(x.at(static_cast<size_t>(i)), beta);
            p[i] = s;
            W[i] = s * (1-s);
        }

        //calculate 1st derivative vector of likelihood function
        first = X.transpose()*(Y - p);

        //calculate Hessian matrix of likelihood function
        hess = -X.transpose()*W.asDiagonal()*X;

        lastBeta = beta;
        try{
        beta = lastBeta - hess.inverse()*first;
        }catch(...){
            throwError(ERROR_SOURCE, "Error while trying to invert matrix in logistic regression");
        }

        bool stop = true;
        VectorXd grad = (beta - lastBeta).array().abs();
        for(int i = 0; i< grad.rows(); i++){
            if(grad[i] > 1e-7){
                stop = false;
                break;
            }
        }

        if(iteration > 50 || stop)
            break;
    }

    if(iteration > 50){
        throwError(ERROR_SOURCE, "Logistic regression failed to converge after 50 iterations.");
    }

    return beta;
}


