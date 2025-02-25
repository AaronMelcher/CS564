SELECT COUNT(DISTINCT Items.SellerID)
FROM Items
JOIN Bids ON Bids.UserID = Items.SellerID;