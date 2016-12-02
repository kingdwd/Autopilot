#include "QPControlAllocation.h"
#include <qpOASES/Constants.hpp>

using Vector2s = QPControlAllocation::Vector2s;
using Vector4s = QPControlAllocation::Vector4s;
using Vector3s = QPControlAllocation::Vector3s;
using Vector7s = QPControlAllocation::Vector7s;
using Matrix2s = QPControlAllocation::Matrix2s;
using Matrix3s = QPControlAllocation::Matrix3s;
using Matrix3x2s = QPControlAllocation::Matrix3x2s;
using Matrix3x7s = QPControlAllocation::Matrix3x7s;
using Matrix7s = QPControlAllocation::Matrix7s;
using ActuatorConfig = QPControlAllocation::ActuatorConfig;
using ActuatorContraints = QPControlAllocation::ActuatorContraints;

QPControlAllocation::QPControlAllocation()
    : isInitialised(false)
    , sqproblem(7,3)
{ }

void QPControlAllocation::init(Vector4s x0_, Matrix2s P_, Matrix3s Q_, Matrix2s Omega_, ActuatorConfig actuatorConfig, ActuatorContraints actuatorConstraints, double qpIterations_)
{
    x0 = x0_;
    P = P_;
    Q = Q_;
    Omega = Omega_;
    config = actuatorConfig;
    constraints = actuatorConstraints;
    qpIterations = qpIterations_;

    isInitialised = true;
    isQPInitialised = false;

    // Create Hessian
    H = Matrix7s::Zero();
    H.topLeftCorner(2,2) = P;
    H.block(2,2,2,2) = Omega;
    H.bottomRightCorner(3,3) = Q;
}

Vector4s QPControlAllocation::operator ()(const Vector3s& tau_desired)
{
    using namespace qpOASES;

    if(isInitialised)
    {
        Vector7s x_prev = Vector7s::Zero();
        x_prev.segment(0,4) = x0;

        Vector2s f = x_prev.segment(2,2);
        Vector2s alpha = x_prev.segment(2,2);
        Vector3s tau = B(alpha)*f;

        int nWSR = 50;//nWSR = 5*(nV+nC) = 5*(7+3)
        double xOpt[7];

        for(int i=0; i < qpIterations; i++)
        {
            Vector7s g = gradient(x_prev);
            Matrix3x7s A = Aeq(x_prev);
            Vector3s b = tau_desired -tau;

            Vector7s lb = lowerBound(x_prev);
            Vector7s ub = upperBound(x_prev);

            if(!isQPInitialised)
            {
                // Set Qudaratic Program
                Options qpOptions;
                qpOptions.enableFlippingBounds = BooleanType::BT_FALSE;
                qpOptions.printLevel = PrintLevel::PL_NONE;

                sqproblem.setOptions(qpOptions);

                sqproblem.init(H.data(),g.data(),A.data(),lb.data(),ub.data(),b.data(),b.data(), nWSR, 0);
                isQPInitialised = true;
            }
            else
            {
                nWSR = 50;
                sqproblem.hotstart(H.data(),g.data(),A.data(),lb.data(),ub.data(),b.data(),b.data(), nWSR, 0);
            }

            if(sqproblem.getPrimalSolution(xOpt) == returnValue::SUCCESSFUL_RETURN)
            {
                Vector7s dx = Vector7s::Map(xOpt);
                x_prev = x_prev + dx; //Acumilate F1, F2, alpha1, alpha2
                x_prev.segment(4,3) = dx.segment(4,3); // Update slack

                f = x_prev.segment(0,2);
                alpha = x_prev.segment(2,2);
                tau = B(alpha)*f;
            }
            else break;
        }

        Vector4s result;
        result << f, alpha;
        return result;

    } throw std::runtime_error("ControlAllocation not initialised");
}



Matrix3x2s QPControlAllocation::B(const Vector2s& alpha)
{
    Matrix3x2s B;
    B <<  std::cos(alpha(0))                                      ,  std::cos(alpha(1)),
          std::sin(alpha(0))                                      ,  std::sin(alpha(1)),
         -config.M1y*std::cos(alpha(0)) + config.M1x*sin(alpha(0)), -config.M2y*std::cos(alpha(1)) + config.M2x*std::sin(alpha(1));
    return B;
}

Matrix3x7s QPControlAllocation::Aeq(const Vector7s& x)
{
    Vector2s f = x.segment(0,2);
    Vector2s alpha = x.segment(2,2);

    Matrix3x2s dBdalpha;
    dBdalpha << -std::sin(alpha(0))                                      , -std::sin(alpha(1)),
                 std::cos(alpha(0))                                      ,  std::cos(alpha(1)),
                 config.M1y*std::sin(alpha(0)) + config.M1x*cos(alpha(0)),  config.M2y*std::sin(alpha(1)) + config.M2x*std::cos(alpha(1));

    Matrix3x7s A;
    A.topLeftCorner(3,2) = B(alpha);
    A.block(0,2,3,1) = dBdalpha.col(0)*f(0);
    A.block(0,3,3,1) = dBdalpha.col(1)*f(1);
    A.topRightCorner(3,3) = Matrix3s::Identity();

    return A;
}

Vector7s QPControlAllocation::gradient(const Vector7s& x)
{
    Vector2s f = x.segment(0,2);
    Vector7s grad = Vector7s::Zero();
    grad.segment(0,2) = P*f;
    return grad;
}

Vector7s QPControlAllocation::lowerBound(const Vector7s& x)
{
    Vector2s alpha0 = x0.segment(2,2);

    Vector2s f = x.segment(0,2);
    Vector2s alpha = x.segment(2,2);

    Vector2s alphamin; alphamin << constraints.alphaMin, constraints.alphaMin;
    Vector2s deltaalphamin; deltaalphamin << constraints.DeltaAlphaMin, constraints.DeltaAlphaMin;

    Vector2s Langle = alphamin - alpha;
    Vector2s Lslew = deltaalphamin - alpha + alpha0;

    Vector7s lb;
    lb <<  constraints.Fmin-f(0),
           constraints.Fmin-f(1),
           std::max(Lslew(0),Langle(0)),
           std::max(Lslew(1),Langle(1)),
          -qpOASES::INFTY,
          -qpOASES::INFTY,
          -qpOASES::INFTY;

    return lb;
}

Vector7s QPControlAllocation::upperBound(const Vector7s& x)
{
    Vector2s alpha0 = x0.segment(2,2);

    Vector2s f = x.segment(0,2);
    Vector2s alpha = x.segment(2,2);

    Vector2s alphamax; alphamax << constraints.alphaMax, constraints.alphaMax;
    Vector2s deltaaplhamax;  deltaaplhamax << constraints.DeltaAlphaMax, constraints.DeltaAlphaMax;

    Vector2s Uangle = alphamax - alpha;
    Vector2s Uslew = deltaaplhamax - alpha + alpha0;

    Vector7s ub;
    ub <<  constraints.Fmax-f(0),
           constraints.Fmax-f(1),
           std::min(Uslew(0),Uangle(0)),
           std::min(Uslew(1),Uangle(1)),
           qpOASES::INFTY,
           qpOASES::INFTY,
           qpOASES::INFTY;

    return ub;
}
