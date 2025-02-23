SELECT COUNT(DISTINCT Items.SellerID)
FROM Items
JOIN Users ON Users.UserID = Items.SellerID
WHERE Users.Rating > 1000;